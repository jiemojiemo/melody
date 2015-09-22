#include "pitchshifting.h"
#include <iostream>
using namespace std;
#define SHORT_TO_FLOAT(x) ((x)/(32768.0))
#define LOG(x) (cout<<x)

template <typename T>
T** New2DArray(int row, int col)
{
	auto array2D = new T*[row];
	for (int i = 0; i < row; ++i)
	{
		array2D[i] = new T[col];
		memset(array2D[i], 0, sizeof(T)*col);
	}

	return array2D;
}

template <typename T>
void Delete2DArray(T** data, int row)
{
	for (int i = 0; i < row; ++i)
	{
		if( data[i] != nullptr )
			delete[] data[i];
	}
	if(data != nullptr)
	delete[] data;
}

void CreateHanningWindow(std::vector<float>& window, int windowSize)
{
	int halfWindowSize = windowSize / 2;
	for (int i = 0; i < windowSize; ++i )
		window.push_back(0.5*(1+cos(PI*(i - halfWindowSize) / (halfWindowSize - 1))));
}

void FreeMemory(const std::vector<std::function<void()>> deleteArray)
{
	for (auto it = deleteArray.begin(); it != deleteArray.end(); ++it)
		*it;
} 

int GetColCount( int i, double scale )
{
	return floor( i*scale );
}
double GetWeight(int i, double scale)
{
	double tmp = i*scale;
	return tmp - floor(tmp);
}

static int saturate(float fvalue, float minval, float maxval) {
	if (fvalue > maxval) {
		fvalue = maxval;
	}
	else if (fvalue < minval) {
		fvalue = minval;
	}
	return (int)fvalue;
}
static void FloatToOtherFormat(const float *bufferIn, void* bufferOut, int64_t numElems,
	int bytesPerSample) {

	if (numElems == 0)
		return;

	void *temp = bufferOut;

	switch (bytesPerSample) {
	case 1: {
		unsigned char *temp2 = (unsigned char *)temp;
		for (int i = 0; i < numElems; i++) {
			temp2[i] = (unsigned char)saturate(bufferIn[i] * 128.0f + 128.0f,
				0.0f, 255.0f);
		}
		break;
	}

	case 2: {
		short *temp2 = (short *)temp;
		for (int i = 0; i < numElems; i++) {
			short value = (short)saturate(bufferIn[i] * 32768.0f, -32768.0f,
				32767.0f);
			temp2[i] = value;
		}
		break;
	}

	case 3: {
		char *temp2 = (char *)temp;
		for (int i = 0; i < numElems; i++) {
			int value = saturate(bufferIn[i] * 8388608.0f, -8388608.0f,
				8388607.0f);
			*((int*)temp2) = value;
			temp2 += 3;
		}
		break;
	}

	case 4: {
		int *temp2 = (int *)temp;
		for (int i = 0; i < numElems; i++) {
			int value = saturate(bufferIn[i] * 2147483648.0f, -2147483648.0f,
				2147483647.0f);
			temp2[i] = value;
		}
		break;
	}
	default:
		//should throw
		break;
	}
}



CPitchShifting::CPitchShifting():
	m_windowSize(1024)
{

}


//************************************
// Method:    TimeScalingAndPitchShifting
// Parameter@ note: 音符的要素
// Parameter@ dataIn: 需要被处理的数据的指针
// Parameter@ dataOut: 指向用于存储处理之后的数据的内存的指针
// Parameter@ sampleCount: 需要被处理的采样个数
// Returns:   void
// Comment:   用于变调和变速，note存放目标要素
// Creator:	  HW
// Modifier:  
//************************************
short* CPitchShifting::TimeScalingAndPitchShifting(const Note & note, short* dataIn,
	unsigned long sampleCount)
{
	assert(sampleCount > 0);

	m_sampleCount = sampleCount;
	int pitch = 44100 / note.frequnce;
	double scale = sampleCount/44100.0/note.time;
	//变速变调处理
	doScalingAndShifting(dataIn, pitch, scale);
	//将结果转化16bits
	short* dataOut = new short[m_finalSampleCount];
	FloatToOtherFormat(m_phaseVocoderOut, dataOut, m_finalSampleCount, 2);
	//释放整个过程中产生的内存（除了dataOut）
	FreeMemory( m_deleter );
	return dataOut;
}



//************************************
// Method:    doScalingAndShifting
// Parameter@ dataIn: 需要被处理的数据的指针
// Parameter@ pitch: 用窗移的方式生成的音高
// Parameter@ scale: 变速的比例，速度变换尺度
// Returns:   void
// Comment:   对音频做变速变调的处理
// Creator:	  HW
// Modifier:  
//************************************
void CPitchShifting::doScalingAndShifting(short * dataIn, int pitch, double scale)
{
	//初始化参数
	Init(pitch, scale);
	//短时傅里叶变化
	STFT(dataIn);
	//相位和幅度变化
	PVProcess();
	//反傅里叶变化，将结果填入dataOut中
	ISTFT();

}

void CPitchShifting::Init(int pitch, double scale)
{
	int hop = pitch;
	//短时傅里叶变换之后的 数据行数
	m_STFTRow = (m_sampleCount - m_windowSize) / hop + 1;
	//短时傅里叶变换之后的 数据列数
	m_STFTCol = m_windowSize / 2 + 1;
	m_hop = hop;
	m_scale = scale;
}

void CPitchShifting::STFT(short * dataIn)
{
	//存放短时傅里叶变换的结果，用于PVProcess
	m_STFTOut = New2DArray<complex>(m_STFTRow, m_STFTCol);
	//汉宁窗
	std::vector<float> hanningWindow;

	//生成汉宁窗
	CreateHanningWindow(hanningWindow, m_windowSize);
	//获取帧数据 = 汉宁窗 * 数据
	int frameCount = 0;

	for (int i = 0; i < m_sampleCount - m_windowSize; i += m_hop)
	{
		//用汉宁窗截取的帧数据
		std::vector<complex> frameData;
		for (int j = 0; j < m_windowSize; ++j)
		{
			double real = SHORT_TO_FLOAT(dataIn[i + j])*hanningWindow[j];
			frameData.push_back(complex(real));
		}
		//做短时傅里叶变化
		CFFT::fft(m_windowSize, &frameData[0]);
		memcpy( m_STFTOut[frameCount], &frameData[0], sizeof(complex)*m_STFTCol );
		++frameCount;
	}
	//将m_STFTOut的内存管理交个deleter
	m_deleter.push_back([&] {LOG("delete m_STFTOut\n"); Delete2DArray(m_STFTOut, m_STFTRow); });
}

//************************************
// Method:    PVProcess
// Returns:   void
// Comment:   进行相位和幅度变化，上游的数据来自m_STFTOut
// Creator:	  HW
// Modifier:  
//************************************
void CPitchShifting::PVProcess()
{
	//变速后的数据长度
	int scaleLength = (m_STFTRow - 2) / m_scale + 1;

	//用于存放相位变换的结果，后面用于ISTFT
	m_PVProcessOut = New2DArray<float>(scaleLength, m_STFTCol);
	//存放相邻的两帧数据,用于计算相位差
	auto adjacentFrames = New2DArray<complex>(2, m_STFTCol);
	ON_SCOPE_EXIT([&]() {Delete2DArray(adjacentFrames, 2); });

	//行的下标
	int colCount  = 0;
	//
	int outColCount = 0;
	//差值的权重
	double weight = 0.0;
	//幅度
	double magnitude = 0.0;
	for (int i = 0; i < scaleLength; ++i,++outColCount)
	{
		colCount = GetColCount(i, m_scale);
		weight = GetWeight( i, m_scale );
		//复制相邻两帧
		memcpy(adjacentFrames[0], m_STFTOut[colCount], m_STFTCol*sizeof(complex));
		memcpy(adjacentFrames[1], m_STFTOut[colCount+1], m_STFTCol*sizeof(complex));

		//进行幅度变化
		for (int j = 0; j < m_STFTCol; ++j)
		{
			magnitude = (1 - weight)*CFFT::c_abs(adjacentFrames[0][j]) + weight*CFFT::c_abs(adjacentFrames[1][j]);
			m_PVProcessOut[outColCount][j] = magnitude;
		}
	}
	//将m_PVProcessOut的内存管理交个deleter
	m_deleter.push_back([&] {LOG("delete m_PVProcessOut\n"); Delete2DArray(m_PVProcessOut, scaleLength); });
}

//************************************
// Method:    ISTFT
// Parameter@ dataIn: 
// Parameter@ PVProcessOut: 
// Returns:   void
// Comment:   反傅里叶变化，进行PV的综合，上游数据来自m_PVProcessOut
// Creator:	  HW
// Modifier:  
//************************************
void CPitchShifting::ISTFT()
{
	
	std::vector<float> synthesizeWindow;
	CreateHanningWindow(synthesizeWindow, m_windowSize);
	//最后输出的帧个数
	int finalCols = (m_STFTRow - 2) / m_scale + 1;
	//最后输出的采样个数
	m_finalSampleCount = m_windowSize + m_hop*(finalCols-1);
	//综合窗之后的数据
	m_phaseVocoderOut = new float[m_finalSampleCount];
	memset(m_phaseVocoderOut, 0, sizeof(float)*m_finalSampleCount );
	//一帧的数据
	float* frameData = new float[m_windowSize];
	ON_SCOPE_EXIT([&]() {delete[] frameData; });

	//取出一帧数据，做反傅里叶变化，叠加合成最后语音输出数据
	for (int i = 0; i < m_hop*(finalCols - 1); i += m_hop)
	{
		//取出一帧数据
		memcpy( frameData, m_PVProcessOut[i/m_hop], sizeof(float)*m_STFTCol );

		//前后调换数据
		for (int ii = m_windowSize - 1; ii > m_windowSize / 2 - 1; --ii)
			frameData[ii] = frameData[m_windowSize - ii - 1];

		//反傅里叶变化
		CFFT::ifft(m_windowSize, frameData);
		//
		CFFT::fftshift(m_windowSize, frameData);
		//进行叠加
		for (int k = 0; k < m_windowSize; ++k)
			m_phaseVocoderOut[i + k] += frameData[k] * synthesizeWindow[k];
	}

	//将m_phaseVocoderOut的内存管理交个deleter
	m_deleter.push_back([&] {LOG("delete m_phaseVocoderOut\n"); delete[] m_phaseVocoderOut; });
}

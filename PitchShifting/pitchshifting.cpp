#include "pitchshifting.h"
#include <iostream>
using namespace std;
#define SHORT_TO_FLOAT(x) ((x)/(32768.0))
#define LOG(x) (cout<<x)

template <typename T>
T** New2DArray(int row, int col)
{
	assert( row > 0 );
	assert( col > 0 );
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
// Parameter@ note: ������Ҫ��
// Parameter@ dataIn: ��Ҫ����������ݵ�ָ��
// Parameter@ dataOut: ָ�����ڴ洢����֮������ݵ��ڴ��ָ��
// Parameter@ sampleCount: ��Ҫ������Ĳ�������
// Returns:   void
// Comment:   ���ڱ���ͱ��٣�note���Ŀ��Ҫ��
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
	assert(pitch > 0);
	assert(scale > 0);
	//cout << "scale:"<<scale <<"pitch:"<<pitch<< endl;

	//���ٱ������
	doScalingAndShifting(dataIn, pitch, scale);
	//�����ת��16bits
	short* dataOut = new short[m_finalSampleCount];
	FloatToOtherFormat(m_phaseVocoderOut, dataOut, m_finalSampleCount, 2);
	//�ͷ����������в������ڴ棨����dataOut��
	FreeMemory( m_deleter );
	return dataOut;
}



//************************************
// Method:    doScalingAndShifting
// Parameter@ dataIn: ��Ҫ����������ݵ�ָ��
// Parameter@ pitch: �ô��Ƶķ�ʽ���ɵ�����
// Parameter@ scale: ���ٵı������ٶȱ任�߶�
// Returns:   void
// Comment:   ����Ƶ�����ٱ���Ĵ���
// Creator:	  HW
// Modifier:  
//************************************
void CPitchShifting::doScalingAndShifting(short * dataIn, int pitch, double scale)
{
	//��ʼ������
	Init(pitch, scale);
	//��ʱ����Ҷ�仯
	STFT(dataIn);
	//��λ�ͷ��ȱ仯
	PVProcess();
	//������Ҷ�仯�����������dataOut��
	ISTFT();

}

void CPitchShifting::Init(int pitch, double scale)
{
	int hop = pitch;
	//��ʱ����Ҷ�任֮��� ��������
	m_STFTRow = (m_sampleCount - m_windowSize) / hop + 1;
	//��ʱ����Ҷ�任֮��� ��������
	m_STFTCol = m_windowSize / 2 + 1;
	m_hop = hop;
	m_scale = scale;
}

void CPitchShifting::STFT(short * dataIn)
{
	//��Ŷ�ʱ����Ҷ�任�Ľ��������PVProcess
	m_STFTOut = New2DArray<complex>(m_STFTRow, m_STFTCol);
	//������
	std::vector<float> hanningWindow;

	//���ɺ�����
	CreateHanningWindow(hanningWindow, m_windowSize);
	//��ȡ֡���� = ������ * ����
	int frameCount = 0;

	for (int i = 0; i < m_sampleCount - m_windowSize; i += m_hop)
	{
		//�ú�������ȡ��֡����
		std::vector<complex> frameData;
		for (int j = 0; j < m_windowSize; ++j)
		{
			double real = SHORT_TO_FLOAT(dataIn[i + j])*hanningWindow[j];
			frameData.push_back(complex(real));
		}
		//����ʱ����Ҷ�仯
		FFT::fft(m_windowSize, &frameData[0]);
		memcpy( m_STFTOut[frameCount], &frameData[0], sizeof(complex)*m_STFTCol );
		++frameCount;
	}
	//��m_STFTOut���ڴ������deleter
	m_deleter.push_back([&] {LOG("delete m_STFTOut\n"); Delete2DArray(m_STFTOut, m_STFTRow); });
}

//************************************
// Method:    PVProcess
// Returns:   void
// Comment:   ������λ�ͷ��ȱ仯�����ε���������m_STFTOut
// Creator:	  HW
// Modifier:  
//************************************
void CPitchShifting::PVProcess()
{
	//���ٺ�����ݳ���
	int scaleLength = (m_STFTRow-2) / m_scale + 1;
	//cout << "scaleLength:" << scaleLength << endl;
	//���ڴ����λ�任�Ľ������������ISTFT
	m_PVProcessOut = New2DArray<float>(scaleLength, m_STFTCol);
	//������ڵ���֡����,���ڼ�����λ��
	auto adjacentFrames = New2DArray<complex>(2, m_STFTCol);
	ON_SCOPE_EXIT([&]() {Delete2DArray(adjacentFrames, 2); });

	//�е��±�
	int colCount  = 0;
	//
	int outColCount = 0;
	//��ֵ��Ȩ��
	double weight = 0.0;
	//����
	double magnitude = 0.0;
	for (int i = 0; i < scaleLength-1; ++i,++outColCount)
	{
		colCount = GetColCount(i, m_scale);
		weight = GetWeight( i, m_scale );
		//����������֡
		memcpy(adjacentFrames[0], m_STFTOut[colCount], m_STFTCol*sizeof(complex));
		memcpy(adjacentFrames[1], m_STFTOut[colCount+1], m_STFTCol*sizeof(complex));

		//���з��ȱ仯
		for (int j = 0; j < m_STFTCol; ++j)
		{
			magnitude = (1 - weight)*FFT::c_abs(adjacentFrames[0][j]) + weight*FFT::c_abs(adjacentFrames[1][j]);
			m_PVProcessOut[outColCount][j] = magnitude;
		}
	}
	//��m_PVProcessOut���ڴ������deleter
	m_deleter.push_back([&] {LOG("delete m_PVProcessOut\n"); Delete2DArray(m_PVProcessOut, scaleLength); });
}

//************************************
// Method:    ISTFT
// Parameter@ dataIn: 
// Parameter@ PVProcessOut: 
// Returns:   void
// Comment:   ������Ҷ�仯������PV���ۺϣ�������������m_PVProcessOut
// Creator:	  HW
// Modifier:  
//************************************
void CPitchShifting::ISTFT()
{
	
	std::vector<float> synthesizeWindow;
	CreateHanningWindow(synthesizeWindow, m_windowSize);
	//��������֡����
	int finalCols = (m_STFTRow - 2) / m_scale + 1;
	//�������Ĳ�������
	m_finalSampleCount = m_windowSize + m_hop*(finalCols-1);
	//�ۺϴ�֮�������
	m_phaseVocoderOut = new float[m_finalSampleCount];
	memset(m_phaseVocoderOut, 0, sizeof(float)*m_finalSampleCount );
	//һ֡������
	float* frameData = new float[m_windowSize];
	ON_SCOPE_EXIT([&]() {delete[] frameData; });

	//ȡ��һ֡���ݣ���������Ҷ�仯�����Ӻϳ���������������
	for (int i = 0; i < m_hop*(finalCols - 1); i += m_hop)
	{
		//ȡ��һ֡����
		memcpy( frameData, m_PVProcessOut[i/m_hop], sizeof(float)*m_STFTCol );

		//ǰ���������
		for (int ii = m_windowSize - 1; ii > m_windowSize / 2 - 1; --ii)
			frameData[ii] = frameData[m_windowSize - ii - 1];

		//������Ҷ�仯
		FFT::ifft(m_windowSize, frameData);
		//
		FFT::fftshift(m_windowSize, frameData);
		//���е���
		for (int k = 0; k < m_windowSize; ++k)
			m_phaseVocoderOut[i + k] += frameData[k] * synthesizeWindow[k];
	}

	//��m_phaseVocoderOut���ڴ������deleter
	m_deleter.push_back([&] {LOG("delete m_phaseVocoderOut\n"); delete[] m_phaseVocoderOut; });

	cout << "  finalSample:"<<m_finalSampleCount << endl;
}

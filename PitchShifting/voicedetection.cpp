#include "voicedetection.h"
#include <assert.h>
#include <memory>
#include <fstream>
#include <numeric>

static int saturate(float fvalue, float minval, float maxval) {
	if (fvalue > maxval) {
		fvalue = maxval;
	}
	else if (fvalue < minval) {
		fvalue = minval;
	}
	return (int)fvalue;
}

static void OtherFormatToFloat(const void* input, float* output, const int64_t BUFF_SIZE,
	int bytesPerSample) {
	switch (bytesPerSample) {
	case 1: {
		unsigned char *temp2 = (unsigned char*)input;
		double conv = 1.0 / 128.0;
		for (int i = 0; i < BUFF_SIZE; i++) {
			output[i] = (float)(temp2[i] * conv - 1.0);
		}
		break;
	}
	case 2: {
		short *temp2 = (short*)input;
		double conv = 1.0 / 32768.0;
		for (int i = 0; i < BUFF_SIZE; i++) {
			short value = temp2[i];
			output[i] = (float)(value * conv);
		}
		break;
	}
	case 3: {
		char *temp2 = (char *)input;
		double conv = 1.0 / 8388608.0;
		for (int i = 0; i < BUFF_SIZE; i++) {
			int value = *((int*)temp2);
			value = value & 0x00ffffff;             // take 24 bits
			value |= (value & 0x00800000) ? 0xff000000 : 0; // extend minus sign bits
			output[i] = (float)(value * conv);
			temp2 += 3;
		}
		break;
	}
	case 4: {
		int *temp2 = (int *)input;
		double conv = 1.0 / 2147483648.0;
		assert(sizeof(int) == 4);
		for (int i = 0; i < BUFF_SIZE; i++) {
			int value = temp2[i];
			output[i] = (float)(value * conv);
		}
		break;
	}
	}
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


CVoiceDetection::CVoiceDetection()
{
	m_winSize = 256;
	m_hop = 90;
	m_ampThreshold = 2;
	m_zcrThreshold = 10;

	m_minSilence = 50;
	m_minVoice = 100;
	m_voiceCount = 0;
	m_silenceCount = 0;

	m_voiseStatus=0;

	m_frameCount = 0;
	m_frameData = NULL;
}
CVoiceDetection::~CVoiceDetection()
{
	if( m_frameData != NULL )
		for( int i = 0; i < m_frameCount; ++i )
		{
			if( m_frameData[i] != NULL )
				delete [] m_frameData[i];
		}
}
vector<SpeechSegment> CVoiceDetection::Detection( const short* buffer, int sampleCount,int sampleRate )
{
    //加窗操作
    EnFrame( buffer, sampleCount, m_winSize, m_hop );
	//计算过零率
    CalcZeroCrossRate();
	//计算能量
    CalcAmplitude();
	//计算能量的门限
	CalcAmpThreshold();
	//计算人声语音段的开始点和结束点，结果在m_startEndMap中
	StartEndPointDetection();
	//返回语音的信息，包括起始终止点、语音的长度(time)
	return GetSpeechSegmentInfo( sampleRate );
}
void CVoiceDetection::EnFrame( const short* dataIn, int sampleCount, int winSize, int hop )
{
	float* floatData = new float[sampleCount];
	ON_SCOPE_EXIT([&]() {delete[] floatData; });
	OtherFormatToFloat(dataIn, floatData, sampleCount, 2);
	//窗的个数
	m_frameCount = (sampleCount - winSize)/hop + 1;
	//构造每个窗的数据
	m_frameData = new float*[ m_frameCount ];
	for( int i = 0; i < m_frameCount; ++i )
	{
		m_frameData[i] = new float[winSize];
		if( m_frameData[i] != NULL )
			memcpy( m_frameData[i], (floatData + i * hop), winSize*sizeof(float) );
	}
}
void CVoiceDetection::CalcZeroCrossRate()
{
    for( int i = 0; i < m_frameCount; ++i )
	{
		int count = 0;
		for( int j = 0; j < m_winSize -1; ++j )
		{
			if( m_frameData[i][j] * m_frameData[i][j + 1] < 0 && m_frameData[i][j] - m_frameData[i][j + 1] > 0.0002)
				count++;
		}
		m_zeroCrossRate.push_back( count );
	}
}
void CVoiceDetection::CalcAmplitude()
{
	for( int i = 0; i < m_frameCount; ++i )
	{
		double ampSum = 0;
		for( int j = 0; j < m_winSize -1; ++j )
		{
			ampSum += m_frameData[i][j] * m_frameData[i][j];
		}
		m_amplitude.push_back( ampSum );
	}
}
void CVoiceDetection::CalcAmpThreshold()
{
	CThreshodCalculator calc( m_amplitude, m_zeroCrossRate );
	m_ampThreshold = calc.GetThreshold();
}

/*
 *	语音端点检测
 *  
 */
void CVoiceDetection::StartEndPointDetection()
{
    int status = 0;
    int start = 0;
    int end = 0;
    int voiceCount = 0;		//语音计数，大于m_minVoice则认为是语音段
    int silenceCount = 0;	//语音间隔计数，如果大于m_minSilence，表明当前语音段结束
    for( int i = 0; i < m_frameCount; ++i )
	{
		switch (status)
		{
		case 0:
		case 1:
			if ( IsVoice( m_amplitude[i], m_zeroCrossRate[i] ) )
			{
				start = MAX( ( i - voiceCount -1 ),1 );
				status = 2;
				silenceCount = 0;
				voiceCount++;
			}
			else if( IsMaybeVoice( m_amplitude[i], m_zeroCrossRate[i] ))  //可能是语音
			{
				status = 1;
				voiceCount++;
			}
			else
			{
				status = 0;
				voiceCount = 0;
			}

			break;
		case 2:
			if( IsVoice( m_amplitude[i], m_zeroCrossRate[i] ) )
			{
				voiceCount++;
			}
			else
			{
				silenceCount++;
				if( silenceCount < m_minSilence )
					voiceCount++;
				else if( voiceCount < m_minVoice )
				{
					status = 0;
					silenceCount = 0;
					voiceCount = 0;
				}
				else
					status = 3;		//当前语音段结束，并且语音段长度足够长，说明是一段人声。记录其端点
			}

			break;
		case 3:
			voiceCount = voiceCount - silenceCount/2;
			end = start + voiceCount - 1;
			m_startEndMap.insert( pair<int,int>( start, end ) );
			status = 0;
			silenceCount = 0;
			voiceCount = 0;
			break;
		}
	}//end for
	//if( voiceCount > m_minVoice )
	//	m_startEndMap.insert( pair<int,int>( m_frameCount - voiceCount,m_frameCount-1 ) );
}
bool CVoiceDetection::IsVoice( double amp, int zcr )
{
	return amp > m_ampThreshold;
}

bool CVoiceDetection::IsMaybeVoice( double amp, int zcr )
{
	return amp > m_ampThreshold || zcr < m_zcrThreshold;
}

//未使用这个函数
vector<SpeechSegment> CVoiceDetection::FindSpeechSegment( const float* buffer, int sampleRate )
{
	assert( buffer != NULL);
	assert( sampleRate != 0 );
	//我们通过amdfSize大小的区域来检查人声频率
	//我们确保amdfSize内至少含有人声两个周期
	int amdfSize = sampleRate / MIN_VOICE_FREQUENCY * 2;
	for( auto it = m_startEndMap.begin(); it != m_startEndMap.end(); ++it )
	{
		//获取人声区域的中间点，确保这个点是人声
		int middleFrameIndex = (it->first + it->second)/2;
		int middleSampleIndex = middleFrameIndex * m_hop;

		vector<float> dataForAMDF( buffer+middleSampleIndex, buffer+middleSampleIndex+amdfSize );
		//计算平均幅度差
		auto amdfResult = AMDFCalc( dataForAMDF );
		//计算人声频率
		auto voiceFrequence = VoiceFrequenceCalc( amdfResult, sampleRate );
		if( 0 != voiceFrequence )
		{
			float beat = float((it->second - it->first)*m_hop)/sampleRate;
			SpeechSegment smg(voiceFrequence, it->first, it->second, beat,0);
			m_speechSegment.push_back( smg );
		}
	}
	return m_speechSegment;
}

vector<SpeechSegment> CVoiceDetection::GetSpeechSegmentInfo(int sampleRate)
{
	assert(sampleRate != 0);
	for (auto it = m_startEndMap.begin(); it != m_startEndMap.end(); ++it)
	{
		float beat = float((it->second - it->first)*m_hop) / sampleRate;
		float velocity = CalcVelocity( it->first, it->second );
		SpeechSegment smg(0, it->first, it->second, beat, velocity);
		m_speechSegment.push_back( smg );
	}
	return m_speechSegment;
}

float CVoiceDetection::CalcVelocity(int start, int end)
{
	auto startIter( m_amplitude.begin() );
	auto endIter( m_amplitude.begin() );
	advance(startIter, start);
	advance(endIter, end);

	return accumulate( startIter, endIter,0.0 )/(end - start);
}


////计算平均幅度差
vector<float> CVoiceDetection::AMDFCalc( const vector<float>& amdfData )
{
	vector<float> amdfResult;
	double maxamdfResult = 0.0;
	for( int i = 0;  i < amdfData.size(); ++i )
	{
		int k=0;
		double sum = 0;
		for( int j = i; j < amdfData.size(); ++j)
		{
			sum += fabs( amdfData[j] - amdfData[k++] );
		}
		amdfResult.push_back( sum );
		//找最大的自相关值
		if( maxamdfResult < sum )
			maxamdfResult = sum;
	}
	//将最大值存放到末尾
	amdfResult.push_back( maxamdfResult );
	return amdfResult;
}

//未使用
int CVoiceDetection::VoiceFrequenceCalc( const vector<float>& amdfResult, int sampleRate )
{
	//获取最大平均幅度差
	double maxamdfResult = amdfResult.back();
	int index = 0;
	
	for( int i = 1; i < amdfResult.size() - 1; ++i )
	{
		//是极小值
		if( amdfResult[i] < amdfResult[i-1] && amdfResult[i]<amdfResult[i+1] )	
			//是真正的谷底，这里采用的方法比较low，应该还有更好的判定方法
			if( amdfResult[i] < maxamdfResult/3.0)								
				//人声频率范围 [1000,80]，这个条件过滤那些被误取的噪音	
				//    F = 1/T
				//    T = i / sampleRate
				if( (sampleRate / i) < MAX_VOICE_FREQUENCY && (sampleRate / i) > 80 )
				{
					index = i;
					break;
				}
	}
	if( 0 == index )
		return 0;
	return sampleRate / index;
}









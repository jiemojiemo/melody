#include "pitchshifting.h"
#include "pcm2wav.h"
#include "voicedetection.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include "CWavread.h"

using namespace std;

//������76�����ģ�ÿ�����ĵ�����
int musicPitch[82] =
{
	0,  63,  0, 63, 0,  63, 0,  63,  0, 63,  0, 63,  0, 63,  0, 67,//ǰ��
	65, 65, 65, 65, 65, 65, 65, 63, 65,  0,  0, 65, 65, 66, 65, 63,
	65, 65, 65, 65, 65, 65, 65, 63, 61, 62, 62, 63, 63, 63, 64, 64, 77,
	0 , 65, 65, 65, 65, 65, 65, 63, 65, 65,  0, 65, 65, 66, 65, 63,
	65, 65, 65, 65, 65, 65, 65, 63, 61, 62, 62, 63, 63, 63, 64, 64, 77,
};
//������76��������ÿ������������Ӧ�ĸ���±�
int lyric[82] =
{
	0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,   0, 1,   0,  1,
	1,  2,  3,  3,  4,  5,  6,  7,  8,  0,  0,  1,  1,   2, 3,   6,
	7,  8,  6,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,   8, 8,   8,   8,
	0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  2,  3,   6, 7,   8,
	8,  1,  1,  1,  1,  1,  1,  1,  1,  2,  3,  4,  5,   5, 6,   7,   8,  

};
//������76�����ģ�ÿ�����ĵ�����Ӧ��ʱ��
int musicTime[82] =
{
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, //ǰ��
	12,  6,  3, 3,  6,   6,  6,  6,  6, 6,  6,  6,  6,  6,  6,  6,
	12,  6,  3, 3,  6,   6,  6,  6,  6, 3,  3,  6,  3,  3,  6,  6,   12,
	12,	 6,  3, 3,  6,   6,  6,  6,  6, 6,  6,  6,  6,  6,  6,  6,
	12,  6,  3, 3,  6,   6,  6,  6,  6, 3,  3,  6,  3,  3,  6,  6,   12,
};

//¼����8�����֣�ÿ���ֶ�Ӧ����ʼλ�ú���ֹλ��
unsigned long voicePosition[16] =
{
	26460,35280,
	35280,44100,
	44100,52920,
	70569,74198,
	74198,82235,
	82235,95850,
	95850,104690,
	107370,116275
};
//��ȡ��Ҫ��Ƶ���ݣ���������Ƭ�εļ��
short* GetDataForDetection(int& sampleCount, int& sampleRate);
//�� wav �ļ���ȡ���ݣ�ֻ�����ڼ������wav�ļ�����������Ƶ���ݴ�0x2cλ�ÿ�ʼ��
short* GetDataForDetectionFromWavFile(const string& fileName, int& sampleCount, int& sampleRate);
//ͨ��¼����ȡ����
short* GetDataForDetectionFromRecord(int& sampleCount, int& sampleRate);
int main()
{
	int sampleCount = 0;
	int sampleRate = 0;
	short* buffer = GetDataForDetection( sampleCount, sampleRate);
	////�������
	//CVoiceDetection detection;
	//auto segments( detection.Detection(buffer,sampleCount,sampleRate) );
	//for (auto& i : segments)
	//	cout << i.start*detection.m_hop << " " << i.end*detection.m_hop << endl;


	float dst_freq = 260;
	float timeScale = 0.0;
	CPitchShifting pitch;
	vector<short> finalOutBuffer;
	vector<pair< unsigned long, unsigned long>> voicePositionInSapmple;

	for (int i = 0; i < 16; i = i + 2)
		voicePositionInSapmple.push_back(pair<unsigned long, unsigned long>(voicePosition[i], voicePosition[i + 1]));


	for (int i = 0; i < 82; i++)
	{
		if (lyric[i] == 0)//������Ϊ�գ�������һ�ξ�����
		{
			unsigned long quiteSize = musicTime[i] * 44100 / 24*0.95;

			for (int k = 0; k < quiteSize; k++)
				finalOutBuffer.push_back(0);
			cout << finalOutBuffer.size() << "  ";
		}
		else
		{
			Note note;
			note.frequnce = 130 * pow(1.0594, (double)(musicPitch[i] - 60));
			note.time = (musicTime[i]) / 24.0*0.95;
			//if (i < 16)
			//{
			//	note.time *= 0.93;
			//}
			note.velocity = 1;
			note.lyric = lyric[i] - 1;
			//�������������ε�λ��
			unsigned long  distance = voicePositionInSapmple[note.lyric].first;
			//�ֵĳ��ȣ����ֵ�sample�ĸ���
			int length = (voicePositionInSapmple[note.lyric].second - voicePositionInSapmple[note.lyric].first);
			auto dataResult = pitch.TimeScalingAndPitchShifting(note, buffer+distance, length);

			for (int k = 0; k < pitch.GetFinalSampleCount(); k++)
				finalOutBuffer.push_back(dataResult[k]);
			for (int kk = pitch.GetFinalSampleCount();kk < note.time * 44100;kk++)
				finalOutBuffer.push_back(0);
			cout << finalOutBuffer.size() << "  ";



		}

	}

	CPcm2Wav convert(&finalOutBuffer[0], finalOutBuffer.size()*sizeof(short), "demo1out.wav");
	Pcm2WavParameter params;
	params.channels = 1;
	params.formatTag = 1;
	params.sampleBits = 16;
	params.sampleRate = 44100;
	convert.Pcm2Wav(params);
}


//************************************
// Method:    GetDataForDetection
// Parameter@ sampleCount: 
// Parameter@ sampleRate: 
// Returns short*:�����ڴ��ָ��
// Comment:   
// Creator:	  HW
// Modifier:  
//************************************
short* GetDataForDetection(int& sampleCount, int& sampleRate)
{
	//return GetDataForDetectionFromRecord(sampleCount, sampleRate);
	return GetDataForDetectionFromWavFile("demo1.wav", sampleCount, sampleRate);
}

//************************************
// Method:    GetDataForDetectionFromWavFile
// Parameter@ fileName: wav�ļ��� 
// Returns short*:   
// Comment:   �ݶ�ֻ�ܶ� 16bit ������wav�ļ�
// Creator:	  HW
// Modifier:  
//************************************
short* GetDataForDetectionFromWavFile(const string& fileName, int& sampleCount, int& sampleRate)
{
	FILE* fd = fopen(fileName.c_str(), "rb");
	if (fd == nullptr)
	{
		fprintf(stderr, "Cannot open the file!\n");
		exit(-1);
	}
	fseek(fd, 0, SEEK_END);
	unsigned long fileSize = ftell(fd) - 0x2c;
	fseek(fd, 0x2c, SEEK_SET);
	short* data = (short*)malloc(fileSize);
	fread(data, 1, fileSize, fd);
	fclose(fd);
	sampleRate = 44100;
	sampleCount = fileSize / sizeof(short);
	return data;
}


//************************************
// Method:    GetDataForDetectionFromRecord
// Returns:   float*
// Comment:   ¼����ʱ��Ϊ 16bit ������ 44.1hz����Ϊʹ������������¼�������ڴ�����һЩ����
// Creator:	  HW
// Modifier:  
//************************************
short* GetDataForDetectionFromRecord(int& sampleCount, int& sampleRate)
{
	////����¼���Ĳ��� ��ʱ��Ϊ 16bit ������ 44.1hz
	//RecordParameters recordParams;
	//recordParams.audioFormat = AUDIO_SIN16;
	//recordParams.framesPerBuffer = 512;
	//recordParams.nChannels = 1;
	//recordParams.nSeconds = 5;
	//recordParams.sampleRate = 44100;

	////��ʼ¼��
	//CRecord<short> recorder(recordParams);
	//recorder.Start();

	////
	//sampleCount = recorder.GetTotalSamples();
	//sampleRate = recordParams.sampleRate;

	////short���� --> float����,���ڴ�����Ҫ��float���ݽ���
	//float* buffer = Bit16toBit32(recorder.GetDataPointer(), recorder.GetTotalSamples());
	//return buffer;
	return nullptr;
}
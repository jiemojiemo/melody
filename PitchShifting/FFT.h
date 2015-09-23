#ifndef FFT_H
#define FFT_H

#include "math.h"

#define PI 3.1415926535897932384626433832795028841971

typedef struct complex //复数类型
{
	complex(float r=0, float i=0) :real(r), imag(i) {}
	float real;		//实部
	float imag;		//虚部
}complex;

class FFT
{
private:
	FFT(const FFT& cp){}
	FFT& operator = (const FFT& cp){}
public:
	static void conjugate_complex(int n,complex in[],complex out[]);//取共轭
	static void c_plus(complex a,complex b,complex *result);//复数加
	static void c_mul(complex a,complex b,complex *result) ;//复数乘
	static void c_sub(complex a,complex b,complex *result);	//复数减法
	static void c_div(complex a,complex b,complex *result);	//复数除法
	static void c_abs(complex f[],float out[],int size);//复数数组取模
	static double c_abs(const complex& f);//复数数组取模
	static void Wn_i(int n,int i,complex *Wn,char flag);//
	static void fftshift(int N,float f[]);
public:
	FFT();
	~FFT();
	static void fft(int N,complex f[]);//傅立叶变换 输出也存在数组f中
	static void ifft(int N,complex f[]); // 傅里叶逆变换	
	static void ifft(int N, float in[]);
};

#endif


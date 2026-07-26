#include "F28x_Project.h"
#include <math.h>
#include <stdlib.h>

#define BUFFER_SIZE 167
#define SAMPLING_DELAY 100
#define SEND_THRESHOLD 30

typedef struct {
    volatile float vBuf[BUFFER_SIZE];
    volatile float iBuf[BUFFER_SIZE];
    float vRMS, iRMS, P, Q, S;
    float vLast, iLast;
    float offsetV, offsetI;
} PhaseData;

PhaseData F1 = {{0}, {0}, 0, 0, 0, 0, 0, 0, 0, 0, 0};
PhaseData F2 = {{0}, {0}, 0, 0, 0, 0, 0, 0, 0, 0, 0};

uint16_t idx = 0, sendCounter = 0;
float suma_P1 = 0, suma_P2 = 0; // <-- VARIABLES ACUMULADORAS AÑADIDAS

void Init_SCIB_9600(void);
void scib_xmit(char a);
void scib_msg(char *msg);
void enviarFloat(float valor);

void main(void) {
    InitSysCtrl();
    InitGpio();
    EALLOW;
    ClkCfgRegs.LOSPCP.all = 0x0002;
    CpuSysRegs.PCLKCR7.bit.SCI_B = 1;
    CpuSysRegs.PCLKCR13.bit.ADC_A = 1; CpuSysRegs.PCLKCR13.bit.ADC_B = 1; CpuSysRegs.PCLKCR13.bit.ADC_C = 1;

    GpioCtrlRegs.GPAGMUX2.bit.GPIO18 = 0; GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 2;
    GpioCtrlRegs.GPAGMUX2.bit.GPIO19 = 0; GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 2;
    GpioCtrlRegs.GPADIR.bit.GPIO31 = 1;
    EDIS;

    DINT; InitPieCtrl(); IER = 0x0000; IFR = 0x0000; InitPieVectTable();

    EALLOW;
    AdcaRegs.ADCCTL2.bit.PRESCALE = 6;
    AdcbRegs.ADCCTL2.bit.PRESCALE = 6;
    AdccRegs.ADCCTL2.bit.PRESCALE = 6;
    AdcSetMode(ADC_ADCA, ADC_RESOLUTION_16BIT, ADC_SIGNALMODE_DIFFERENTIAL);
    AdcSetMode(ADC_ADCB, ADC_RESOLUTION_16BIT, ADC_SIGNALMODE_DIFFERENTIAL);
    AdcSetMode(ADC_ADCC, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);

    AdcaRegs.ADCSOC0CTL.bit.CHSEL = 0;  AdcaRegs.ADCSOC0CTL.bit.ACQPS = 150;
    AdcbRegs.ADCSOC0CTL.bit.CHSEL = 2;  AdcbRegs.ADCSOC0CTL.bit.ACQPS = 150;
    AdccRegs.ADCSOC0CTL.bit.CHSEL = 4;  AdccRegs.ADCSOC0CTL.bit.ACQPS = 150;
    AdccRegs.ADCSOC1CTL.bit.CHSEL = 5;  AdccRegs.ADCSOC1CTL.bit.ACQPS = 150;

    AdccRegs.ADCINTSEL1N2.bit.INT1SEL = 1; AdccRegs.ADCINTSEL1N2.bit.INT1E = 1;
    AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1; AdcbRegs.ADCCTL1.bit.ADCPWDNZ = 1; AdccRegs.ADCCTL1.bit.ADCPWDNZ = 1;
    EDIS;

    Init_SCIB_9600();
    DELAY_US(100000);

    while(1) {
        AdcaRegs.ADCSOCFRC1.bit.SOC0 = 1; AdcbRegs.ADCSOCFRC1.bit.SOC0 = 1;
        AdccRegs.ADCSOCFRC1.bit.SOC0 = 1; AdccRegs.ADCSOCFRC1.bit.SOC1 = 1;

        while(AdccRegs.ADCINTFLG.bit.ADCINT1 == 0);
        AdccRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;

        float v1_r = (((float)AdcaResultRegs.ADCRESULT0 - 32768.0f) * (3.0f/32768.0f) * 92.88f) + 4.57f;
        float i1_r = ((float)AdccResultRegs.ADCRESULT1 * (3.0f/4096.0f) - 1.25f) * 40.0f;
        float v2_r = (((float)AdcbResultRegs.ADCRESULT0 - 32768.0f) * (3.0f/32768.0f) * 92.88f) + 4.57f;
        float i2_r = ((float)AdccResultRegs.ADCRESULT0 * (3.0f/4096.0f) - 1.25f) * 40.0f;

        F1.vLast = (0.4f * v1_r) + (0.6f * F1.vLast);
        F1.iLast = (0.2f * i1_r) + (0.8f * F1.iLast);
        F1.vBuf[idx] = F1.vLast - F1.offsetV;
        F1.iBuf[idx] = F1.iLast - F1.offsetI;

        F2.vLast = (0.4f * v2_r) + (0.6f * F2.vLast);
        F2.iLast = (0.2f * i2_r) + (0.8f * F2.iLast);
        F2.vBuf[idx] = F2.vLast - F2.offsetV;
        F2.iBuf[idx] = F2.iLast - F2.offsetI;

        idx++;

        if(idx >= BUFFER_SIZE) {
            float sV1=0, sI1=0, sP1=0, sumV1=0, sumI1=0;
            float sV2=0, sI2=0, sP2=0, sumV2=0, sumI2=0;
            uint16_t j;
            for(j=0; j<BUFFER_SIZE; j++) {
                sumV1 += (F1.vBuf[j] + F1.offsetV); sumI1 += (F1.iBuf[j] + F1.offsetI);
                sumV2 += (F2.vBuf[j] + F2.offsetV); sumI2 += (F2.iBuf[j] + F2.offsetI);
            }
            F1.offsetV = sumV1/167.0f; F1.offsetI = sumI1/167.0f;
            F2.offsetV = sumV2/167.0f; F2.offsetI = sumI2/167.0f;

            for(j=0; j<BUFFER_SIZE; j++) {
                sV1 += F1.vBuf[j]*F1.vBuf[j]; sI1 += F1.iBuf[j]*F1.iBuf[j]; sP1 += F1.vBuf[j]*F1.iBuf[j];
                sV2 += F2.vBuf[j]*F2.vBuf[j]; sI2 += F2.iBuf[j]*F2.iBuf[j]; sP2 += F2.vBuf[j]*F2.iBuf[j];
            }

            F1.vRMS = sqrtf(sV1/167.0f); F1.iRMS = sqrtf(sI1/167.0f); F1.P = -(sP1/167.0f);
            F1.S = F1.vRMS * F1.iRMS;
            float d1 = (F1.S*F1.S) - (F1.P*F1.P); F1.Q = (d1 > 0) ? sqrtf(d1) : 0;
            if(F1.iRMS < 0.15f) { F1.iRMS = 0; F1.P = 0; F1.Q = 0; F1.S = 0; }

            F2.vRMS = sqrtf(sV2/167.0f); F2.iRMS = sqrtf(sI2/167.0f); F2.P = -(sP2/167.0f);
            F2.S = F2.vRMS * F2.iRMS;
            float d2 = (F2.S*F2.S) - (F2.P*F2.P); F2.Q = (d2 > 0) ? sqrtf(d2) : 0;
            if(F2.iRMS < 0.15f) { F2.iRMS = 0; F2.P = 0; F2.Q = 0; F2.S = 0; }

            // <-- ACUMULACIÓN AÑADIDA AQUÍ -->
            suma_P1 += F1.P;
            suma_P2 += F2.P;

            idx = 0; sendCounter++;

            if(sendCounter >= SEND_THRESHOLD) {
                // <-- CÁLCULO DE PROMEDIOS AÑADIDO AQUÍ -->
                float P1_promedio = suma_P1 / (float)SEND_THRESHOLD;
                float P2_promedio = suma_P2 / (float)SEND_THRESHOLD;

                enviarFloat(F1.vRMS); scib_xmit(','); enviarFloat(F1.iRMS); scib_xmit(',');
                enviarFloat(P1_promedio); scib_xmit(','); enviarFloat(F1.Q); scib_xmit(','); // Se envía P1_promedio
                enviarFloat(F1.S);    scib_xmit(',');

                enviarFloat(F2.vRMS); scib_xmit(','); enviarFloat(F2.iRMS); scib_xmit(',');
                enviarFloat(P2_promedio); scib_xmit(','); enviarFloat(F2.Q); scib_xmit(','); // Se envía P2_promedio
                enviarFloat(F2.S);    scib_xmit('\n');

                GpioDataRegs.GPATOGGLE.bit.GPIO31 = 1;
                sendCounter = 0;

                // <-- REINICIO DE ACUMULADORES AÑADIDO AQUÍ -->
                suma_P1 = 0;
                suma_P2 = 0;
            }
        }
        DELAY_US(SAMPLING_DELAY);
    }
}

void enviarFloat(float valor) {
    long pE = (long)valor; float res = valor - (float)pE; if(res < 0) res *= -1.0f;
    long pD = (long)(res * 100.0f); char buf[16]; ltoa(pE, buf, 10);
    scib_msg(buf); scib_xmit('.'); if(pD < 10) scib_xmit('0');
    ltoa(pD, buf, 10); scib_msg(buf);
}
void Init_SCIB_9600(void) {
    ScibRegs.SCICCR.all = 0x0007; ScibRegs.SCICTL1.all = 0x0003;
    ScibRegs.SCIHBAUD.all = (650 >> 8); ScibRegs.SCILBAUD.all = (650 & 0xFF);
    ScibRegs.SCICTL1.all = 0x0023;
}
void scib_xmit(char a) { while (ScibRegs.SCICTL2.bit.TXRDY == 0); ScibRegs.SCITXBUF.all = a; }
void scib_msg(char *msg) { while(*msg != '\0') { scib_xmit(*msg); msg++; } }

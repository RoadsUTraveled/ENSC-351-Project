#include "sampler.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h> // For sleep function
#include <string.h> // For memset function
#include <stdbool.h>
#include <stdio.h>
#include "common.h"

#define A2D_FILE_VOLTAGE0 "/sys/bus/iio/devices/iio:device0/in_voltage1_raw"
#define A2D_VOLTAGE_REF_V 1.8
#define A2D_MAX_READING 4095

// Assuming a fixed-size buffer for simplicity
#define MAX_SAMPLES 1000

static samplerDatapoint_t sampleHistory[MAX_SAMPLES];
static int sampleCount = 0;
static long long totalSamples = 0;
static pthread_mutex_t mutex;
static pthread_t samplerThread;
static int running = 0;

double getVoltage0Reading()
{
    // Open file
    FILE *f = fopen(A2D_FILE_VOLTAGE0, "r");
    if (!f) {
        printf("ERROR: Unable to open voltage input file. Cape loaded?\n");
        printf(" Check /boot/uEnv.txt for correct options.\n");
        exit(-1);
    }
    // Get reading
    int a2dReading = 0;
    int itemsRead = fscanf(f, "%d", &a2dReading);
    if (itemsRead <= 0) {
        printf("ERROR: Unable to read values from voltage input file.\n");
        exit(-1);
    }
    // Close file
    fclose(f);
    double voltage = ((double)a2dReading / A2D_MAX_READING) * A2D_VOLTAGE_REF_V;
    return voltage;
}

void* samplingFunction(void* arg) {
    long long startTime = getTimeInMicros();    
    long long currentTime = 0;
    long long timeelapsed = 0;

    while (running) {
        pthread_mutex_lock(&mutex);
        currentTime = getTimeInMicros();
        timeelapsed = currentTime - startTime;

        if (sampleCount < MAX_SAMPLES && timeelapsed <= 1000) {
            samplerDatapoint_t newSample;
            newSample.sampleInV = getVoltage0Reading();
            newSample.timestampInNanoS = getTimeInMicros();
            sampleHistory[sampleCount++] = newSample;
            totalSamples++;
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void Sampler_startSampling(void) {
    running = 1;
    pthread_mutex_init(&mutex, NULL);
    pthread_create(&samplerThread, NULL, samplingFunction, NULL);
}

void Sampler_stopSampling(void) {
    running = 0;
    pthread_join(samplerThread, NULL);
    pthread_mutex_destroy(&mutex);
}

samplerDatapoint_t* Sampler_extractAllValues(int *length) {
    pthread_mutex_lock(&mutex);

    *length = sampleCount;
    samplerDatapoint_t* samples = malloc(sampleCount * sizeof(samplerDatapoint_t));
    memcpy(samples, sampleHistory, sampleCount * sizeof(samplerDatapoint_t));
    sampleCount = 0; 

    pthread_mutex_unlock(&mutex);
    return samples;
}

int Sampler_getNumSamplesInHistory() {
    pthread_mutex_lock(&mutex);
    int count = sampleCount;
    pthread_mutex_unlock(&mutex);
    return count;
}

long long Sampler_getNumSamplesTaken(void) {
    return totalSamples; // Assuming this doesn't need to be thread-safe
}


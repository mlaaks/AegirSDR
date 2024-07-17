%AegirSDR
%
%Matlab script demonstrating CZMQSDR

%Receive samples, examine spectrum (in realtime if possible, but zmq-
%buffers can and will stretch if matlab polls the socket too slowly).

clear all;close all;

sdr = CZMQSDR('IPAddress','127.0.0.1');
FESR = 240e3;
NDEC = 5;
nSample = 2^16;
scope = dsp.SpectrumAnalyzer(...
    'Name',             'Spectrum',...
    'Title',            'Spectrum', ...
    'SpectrumType',     'Power',...
    'FrequencySpan',    'Full', ..
    'SampleRate',        FESR, ...
    'YLimits',          [-50,5],...
    'SpectralAverages', 50, ...
    'FrequencySpan',    'Start and stop frequencies', ...
    'StartFrequency',   -FESR/2, ...
    'StopFrequency',    FESR/2);

addpath('/u/53/laaksom17/unix/source/SoftGNSS');
addpath('/u/53/laaksom17/unix/source/SoftGNSS/geoFunctions');
addpath('/u/53/laaksom17/unix/source/SoftGNSS/include');

settings=initSettings();
samplesPerCode = round(settings.samplingFreq / ...
            (settings.codeFreqBasis / settings.codeLength));

%settings.acquisition.cohCodePeriods*settings.acquisition.nonCohSums+1)*samplesPerCode*6
L1frequency=1575.42e6;
sdr();
%sdr.CenterFrequency = L1frequency+settings.IF;
%fname = 'iq.bin';
sdr.CenterFrequency = 106.2e6;
%delete(fname);
hAudio = audioDeviceWriter(FESR/NDEC,'BufferSize',ceil(nSample*2/NDEC));
while 1
    x = sdr();
    x = x - mean(x);
    scope(x);
    %bbw(x(:,1));
    %write8bitIQ(x(:,3),fname);
    %write32bitIQ(x(:,3),fname);
    %data = x(1:(settings.acquisition.cohCodePeriods*settings.acquisition.nonCohSums+1)*samplesPerCode*3,1);
    %acqResults = acquisition(data.', (settings);
    %z = fmdemod(y,Fc,fscanf3e6,75000)
    s = x(2:end,2);
    sd = x(1:end-1,2);

    desic = angle(s.*conj(sd));
    desic = resample(desic,1,5);
    hAudio(desic);
end

release(sdr);
release(bbw);

function n = write8bitIQ(x,fname)
    file = fopen(fname, 'a');
    
    xinterleaved(1:2:size(x,1)*2,:) = real(x);dedesicsic
    xinterleaved(2:2:size(x,1)*2,:) = imag(x);
    
    n = fwrite(file, int8(127.0*xinterleaved), 'int8');
    fclose(file);
end

function n = write32bitIQ(x,fname)
    file = fopen(fname,'a');
    xinterleaved(1:2:size(x,1)*2,:) = real(x);
    xinterleaved(2:2:size(x,1)*2,:) = imag(x);
    
    n = fwrite(file, single(127.0*xinterleaved), 'single');
    fclose(file);
end

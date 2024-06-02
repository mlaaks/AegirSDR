clear all; close all;

ymax = 100;
figure('units','normalized','outerposition',[0 0 1 1]);
Nstart = 1;
Nend   = 8192;
for n=1:48
    load(['meas' num2str(n) '.mat']);
    X = X(Nstart:Nend,:);
    
    Np = size(X(:,:,1),2);
    Nr = round(Np/4);

    for k=1:Np
        subplot(Nr,4,k);
        c=xcorr(X(:,3),X(:,k));
        stem(c.*conj(c));
        xlim([0 (2*size(X,1)-1)]);
        ylim([0 ymax]);

        %PAPR. Crest factor could also be used.
        PAPR = abs(max(c))^2/rms(c)^2;
        title(['meas' num2str(n) '.mat, PAPR: ' num2str(PAPR)]);
    end
    pause(1);    
end
%%

n=9;
load(['meas' num2str(n) '.mat']);
Xall=X;
L = 8192;

X = X(L*1+1:end-L,:);
X(:,11:end) = Xall(Nstart+L:Nend+L,11:end);

Np = size(X(:,:,1),2);
Nr = round(Np/4);

for k=1:Np
    subplot(Nr,4,k);
    c=xcorr(X(:,2),X(:,k));
    stem(c.*conj(c));
    xlim([0 (2*size(X,1)-1)]);
    ylim([0 ymax]);

    %PAPR. Crest factor could also be used.
    PAPR = abs(max(c))^2/rms(c)^2;
    title(['meas' num2str(n) '.mat, PAPR: ' num2str(PAPR)]);
end    

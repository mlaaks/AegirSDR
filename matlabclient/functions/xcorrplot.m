%Plot the cross-correlations of signal channels against reference channel
%
function [o] = xcorrplot(X,seq)
    Np = size(X(:,:,1),2);
    Nr = round(Np/4);

    %figure('units','normalized','outerposition',[0 0 1 1]);
    for k=1:Np
    subplot(Nr,4,k);
    c=xcorr(X(:,1),X(:,k));
    stem(c.*conj(c));
    xlim([0 (2*size(X,1)-1)]);
    %ylim([0 ymax]);

    %PAPR. Crest factor could also be used.
    PAPR = abs(max(c))^2/rms(c)^2;
    %title(['seq: ' num2str(seq(n,k)) ' PAPR: ' num2str(PAPR)]);
    title(['PAPR: ' num2str(PAPR)]);
    end
end


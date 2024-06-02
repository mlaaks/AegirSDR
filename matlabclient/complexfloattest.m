clear all; close all;

%mq = zmq('subscribe', 'tcp', '192.168.1.21', 5555);
mq = zmq('subscribe', 'tcp', '127.0.0.1', 5555);

recz=[];
for n=1:10
     samples = zmq('receive', mq);
     re_im = reshape(typecast(samples, 'single'),length(samples)/(8*5),[]);
     z = complex(re_im(1,:), re_im(2,:));
end



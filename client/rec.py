#import pygame
import numpy
import struct
import pyaudio
import threading

class SwhRecorder:
    """Simple, cross-platform class to record from the microphone."""
    
    def __init__(self):
        """minimal garb is executed when class is loaded."""
        self.RATE=48000
        self.BUFFERSIZE=2048 #1024 is a good buffer size
        self.secToRecord=.2
        self.threadsDieNow=False
        self.newAudio=False
        self.fftOK=False
        
    def setup(self, index):
        """initialize sound card."""
        #TODO - windows detection vs. alsa or something for linux
        #TODO - try/except for sound card selection/initiation

        self.buffersToRecord=int(self.RATE*self.secToRecord/self.BUFFERSIZE)
        if self.buffersToRecord==0: self.buffersToRecord=1
        self.samplesToRecord=int(self.BUFFERSIZE*self.buffersToRecord)
        self.chunksToRecord=int(self.samplesToRecord/self.BUFFERSIZE)
        self.secPerPoint=1.0/self.RATE
        
        self.p = pyaudio.PyAudio()
        self.inStream = self.p.open(format=pyaudio.paInt16,channels=1,rate=self.RATE,input=True,input_device_index=index,
                       frames_per_buffer=self.BUFFERSIZE)
        
        self.xsBuffer=numpy.arange(self.BUFFERSIZE)*self.secPerPoint
        self.xs=numpy.arange(self.chunksToRecord*self.BUFFERSIZE)*self.secPerPoint
        self.audio=numpy.empty((self.chunksToRecord*self.BUFFERSIZE),dtype=numpy.int16)               
    
    def close(self):
        """cleanly back out and release sound card."""
        self.p.close(self.inStream)
    
    ### RECORDING AUDIO ###  
    
    def getAudio(self):
        """get a single buffer size worth of audio."""
        success = False
        while not success:
            try:
                audioString=self.inStream.read(self.BUFFERSIZE)
                success = True
            except IOError:
                success = False

        return numpy.fromstring(audioString,dtype=numpy.int16)
        
    def record(self,forever=True):
        """record secToRecord seconds of audio."""
        while True:
            if self.threadsDieNow: break
            for i in range(self.chunksToRecord):
                self.audio[i*self.BUFFERSIZE:(i+1)*self.BUFFERSIZE]=self.getAudio()
            self.newAudio=True 
            if forever==False: break
    
    def continuousStart(self):
        """CALL THIS to start running forever."""
        self.t = threading.Thread(target=self.record)
        self.t.start()
        
    def continuousEnd(self):
        """shut down continuous recording."""
        self.threadsDieNow=True

    ### MATH ###
            
    def downsample(self,data,mult):
        """Given 1D data, return the binned average."""
        overhang=len(data)%mult
        if overhang: data=data[:-overhang]
        data=numpy.reshape(data,(len(data)/mult,mult))
        data=numpy.average(data,1)
        return data    

    #@profile
    def fft(self, minHz=200, maxHz=8000):
        numpy.set_printoptions(threshold=numpy.nan)
        data=self.audio.flatten()
        rate = self.RATE / 2
        factor = 1
        while rate > maxHz * 2:
            rate /= 2
            factor *= 2
        if factor > 1:
            data = self.downsample(data, factor)
        #print(factor, rate)
        left,right=numpy.split(numpy.abs(numpy.fft.fft(data)),2)
        ys=numpy.add(left,right[::-1])
#        ys = numpy.multiply(ys,numpy.hamming(len(ys)))
#        print self.RATE

        if not self.fftOK:
            xs=numpy.linspace(0,rate, num=len(ys), endpoint=False)
            base = 20
            logbase = numpy.log(base)
            logmin = numpy.log(minHz)/logbase
            logmax = numpy.log(maxHz)/logbase
            self.bands = 64
            logxs = numpy.logspace(logmin, logmax, num=self.bands+1, base = base, endpoint=True).astype(int)
            self.logxs = logxs

            self.bandstart = [0] * self.bands
            self.bandend = [0] * self.bands
            self.counts = [0] * self.bands
            lin = 0
            while xs[lin] < logxs[0]:
                lin += 1
            for i in range(self.bands):
                self.bandstart[i] = lin
                while lin < len(xs) and xs[lin] < logxs[i + 1]:
                    lin += 1
                if lin == self.bandstart[i]:
                    self.bandend[i] = lin + 1
                else:
                    self.bandend[i] = lin
                self.counts[i] = self.bandend[i] - self.bandstart[i]

            # redo wih no endpoint
            self.fftOK = True

        
        out = numpy.zeros(self.bands)

        for i in range(self.bands):
            out[i] = ys[self.bandstart[i]:self.bandend[i]].sum() / self.counts[i]

        #if logScale:
        out=numpy.multiply(20,numpy.log10(out))
#        out=numpy.multiply(out,self.bandmul)
            
        #print len(logxs), len(out)
        #print out
        return out
            #        return xs,ys

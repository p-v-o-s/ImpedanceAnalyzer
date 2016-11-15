import glob, time
import serial
import numpy as np

class IA(object):
    def __init__(self, port):
        self._ser = serial.Serial(port=port)
    
    def send(self, cmd):
        cmd += "\n"
        self._ser.write(cmd.encode())
        
    def calibrate(self,
                  ref_resistance,
                  start_freq = 1000,
                  freq_incr  = 10000,
                  num_points = 100,
                  ):
        self.send("IA.START_FREQ %d" % start_freq)
        self.send("IA.FREQ_INCR %d" % freq_incr)
        self.send("IA.NUM_INCR %d" % num_points)
        self.send("IA.SWEEP.INIT!")
        self.send("IA.SWEEP.START!")
        data = []
        for i in range(num_points):
            self.send("IA.SWEEP.MEAS_RAW?\n")
            resp = self._ser.readline().decode()
            #print(resp)
            data.append(list(map(float,resp.split(","))))
            self.send("IA.SWEEP.INCR_FREQ!\n")
        data = np.array(data)
        f = data[:,0]
        R = data[:,1]
        I = data[:,2]
        gain  = (1.0/ref_resistance)/np.sqrt(R**2 + I**2)
        phase = np.arctan2(I,R)
        return (f, gain, phase)


################################################################################
# main
################################################################################
if __name__ == "__main__":
    port = glob.glob("/dev/ttyACM*")[-1]
    print(port)
    
    ia = IA(port)
    f, gain, phase = ia.calibrate(1000)

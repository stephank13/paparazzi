import socket;
import os;
ip_address = socket.gethostbyname("gfi3101365");

#os.chdir("/home/toughone/paparazzi_sumo/var/logs")
log_files = os.listdir("/home/toughone/paparazzi_sumo/var/logs")
data_files = [s for s in log_files if ".data" in s]

data_files.sort()

most_recent_data_file = data_files[len(data_files)-1]

command = "tail --lines=100 -f /home/toughone/paparazzi_sumo/var/logs/%s | pv -b | nc -v %s 1234" % (most_recent_data_file,ip_address)
#print most_recent_data_file
#print type(ip_address)
#print type(data_files[len(data_files)-1])
print command
os.system(command)

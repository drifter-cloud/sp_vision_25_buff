sleep 5
cd ~/Desktop/sp_vision_25/
source /opt/ros/humble/setup.bash
source /home/rm/Desktop/sp_msg_25/install/setup.bash
export LD_LIBRARY_PATH=/opt/ros/humble/lib:/home/rm/Desktop/sp_msg_25/install/sp_msgs/lib:$LD_LIBRARY_PATH
gnome-terminal -x bash -c "./build/sentry_multithread;exec bash"
# screen \
#     -L \
#     -Logfile logs/$(date "+%Y-%m-%d_%H-%M-%S").screenlog \
#     -d \
#     -m \
#     bash -c "export LD_LIBRARY_PATH=/opt/ros/humble/lib:/home/rm/Desktop/sp_msg_25/install/lib:$LD_LIBRARY_PATH; ./build/sentry_multithread configs/sentry.yaml"

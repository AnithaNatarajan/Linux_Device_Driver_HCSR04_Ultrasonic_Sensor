echo -n "24" > /sys/class/gpio/export
echo -n "out" > /sys/class/gpio/gpio24/direction

#exporting mux pin
echo -n "44" > /sys/class/gpio/export
echo -n "0" > /sys/class/gpio/gpio44/value

echo -n "5" > /sys/class/HCSR/HCSRdevice1/trigger
#-------------------------------------------

#echo pin setting---------------------------
#exporting direction pin
echo -n "26" > /sys/class/gpio/export
echo -n "in" > /sys/class/gpio/gpio26/direction

#exporting mux pin
echo -n "74" > /sys/class/gpio/export 
echo -n "0" > /sys/class/gpio/gpio74/value
echo -n "10" > /sys/class/HCSR/HCSRdevice1/echo

echo -n "0" > /sys/class/HCSR/HCSRdevice1/mode   
echo -n "30" > /sys/class/HCSR/HCSRdevice1/frequency
echo -n "1" > /sys/class/HCSR/HCSRdevice1/enable


cat           /sys/class/HCSR/HCSRdevice1/trigger   
cat           /sys/class/HCSR/HCSRdevice1/echo 
cat           /sys/class/HCSR/HCSRdevice1/mode
cat           /sys/class/HCSR/HCSRdevice1/frequency
cat           /sys/class/HCSR/HCSRdevice1/enable
sleep 1
cat           /sys/class/HCSR/HCSRdevice1/distance

echo -n "0" > /sys/class/HCSR/HCSRdevice1/enable  
echo -n "1" > /sys/class/HCSR/HCSRdevice1/mode
echo -n "1" > /sys/class/HCSR/HCSRdevice1/enable
sleep 10
echo -n "0" > /sys/class/HCSR/HCSRdevice1/enable 

##########################


echo -n "36" > /sys/class/gpio/export
echo -n "out" > /sys/class/gpio/gpio36/direction


echo -n "6" > /sys/class/HCSR/HCSRdevice2/trigger
#-------------------------------------------

#echo pin setting---------------------------
#exporting direction pin
echo -n "20" > /sys/class/gpio/export
echo -n "in" > /sys/class/gpio/gpio20/direction

#exporting mux pin
echo -n "68" > /sys/class/gpio/export 
echo -n "0" > /sys/class/gpio/gpio68/value
echo -n "1" > /sys/class/HCSR/HCSRdevice2/echo

echo -n "0" > /sys/class/HCSR/HCSRdevice2/mode   
echo -n "30" > /sys/class/HCSR/HCSRdevice2/frequency
echo -n "1" > /sys/class/HCSR/HCSRdevice2/enable


cat           /sys/class/HCSR/HCSRdevice2/trigger   
cat           /sys/class/HCSR/HCSRdevice2/echo 
cat           /sys/class/HCSR/HCSRdevice2/mode
cat           /sys/class/HCSR/HCSRdevice2/frequency
cat           /sys/class/HCSR/HCSRdevice2/enable
sleep 1
cat           /sys/class/HCSR/HCSRdevice2/distance

echo -n "0" > /sys/class/HCSR/HCSRdevice2/enable  
echo -n "1" > /sys/class/HCSR/HCSRdevice2/mode
echo -n "1" > /sys/class/HCSR/HCSRdevice2/enable
sleep 10
echo -n "0" > /sys/class/HCSR/HCSRdevice2/enable 












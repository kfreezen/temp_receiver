OS=`lsb_release -si`

INSTALLER=opkg

case $OS in
	"Angstrom")
		INSTALLER=opkg
		UBOOT_UENV=/boot/uEnv.txt
		REMOVE_LIST="firefox gimp chrome xfce-terminal"
		WORKING_DIR=/home/root/receiver
		EXEC_PATH=$WORKING_DIR/receiver.sh
		;;
	"Debian")
		INSTALLER=apt-get
		UBOOT_UENV=/boot/uboot/uEnv.txt
		REMOVE_LIST="firefox gimp chromium"
		WORKING_DIR=/root/receiver
		EXEC_PATH=$WORKING_DIR/receiver.sh
		;;
	*)
		echo Error Occurred.
		exit
esac
	
$INSTALLER remove $REMOVE_LIST
echo optargs=text > $UBOOT_UENV

cat > /lib/systemd/system/receiver.service <<EOF
[Unit]
Description=Start Temp Receiver

[Service]
Type=simple
WorkingDirectory=$WORKING_DIR
ExecStart=$EXEC_PATH

[Install]
WantedBy=default.target
EOF

systemctl enable receiver.service

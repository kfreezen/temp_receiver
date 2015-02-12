OS=`lsb_release -si`

INSTALLER=opkg

case $OS in
	"Angstrom")
		INSTALLER=opkg
		UBOOT_UENV=/boot/uEnv.txt
		REMOVE_LIST="firefox gimp chrome xfce-terminal"
		;;
	"Debian")
		INSTALLER=apt-get
		UBOOT_UENV=/boot/uboot/uEnv.txt
		REMOVE_LIST="firefox gimp chromium"
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
WorkingDirectory=/home/root/receiver
ExecStart=/home/root/receiver/receiver.sh

[Install]
WantedBy=default.target
EOF

systemctl daemon-reload

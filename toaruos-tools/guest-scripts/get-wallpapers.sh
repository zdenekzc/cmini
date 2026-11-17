# get-wallpapers.sh - classic ToaruOS wallpapers from GitHub into /usr/share/wallpapers
#   (run IN ToaruOS:  sh /tmp/cecko/get-wallpapers.sh ; as root - in the window
#   as user local:  sudo sh /tmp/cecko/get-wallpapers.sh, password local)
#
# fetch in the base ToaruOS 2.3.2 only speaks HTTP ("no tls backend") and GitHub
# redirects HTTP to HTTPS => first the fetch-tls package (msk uses HTTP, confirmation
# "y" from the pipe); then the fetch commands can use HTTPS too. The commands are literal (esh has no
# loops). The wallpaper is picked in the desktop settings (right-click on the desktop).
echo y | msk install fetch-tls
fetch -o /usr/share/wallpapers/hanging.jpg https://raw.githubusercontent.com/klange/toaruos/v2.2.0/base/usr/share/wallpapers/hanging.jpg
fetch -o /usr/share/wallpapers/nagoya.jpg https://raw.githubusercontent.com/klange/toaruos/v2.1.0/base/usr/share/wallpapers/nagoya.jpg
fetch -o /usr/share/wallpapers/flowers.jpg https://raw.githubusercontent.com/klange/toaruos/v2.1.0/base/usr/share/wallpapers/flowers.jpg
ls -l /usr/share/wallpapers

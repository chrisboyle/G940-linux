# G940-linux
Improvements to Linux kernel support for
[Logitech Flight System G940](https://support.logitech.com/en_gb/product/flight-system-g940)

This device has partial Linux support since 2010, but with bugs including some axes conflated/missing.

I have made both some relatively uncontroversial improvements,
[submitted upstream](https://patchwork.kernel.org/project/linux-input/list/?series=90297)
(fix axis/button mappings, support LEDs, remove hard autocenter) and some more experimental changes which are only here, to
support additional force feedback effects by including
[ff-memless-next from Michal Malý](https://patchwork.kernel.org/project/linux-input/list/?q=ff-memless-next&archive=both),
a sort of emulation layer for periodic and rumble effects which was submitted upstream circa 2014, never merged, but it still
works great for this.

My goal for the ff-memless-next stuff and any other changes less acceptable to upstream is to make a DKMS package with altered
module name(s) and modprobe blacklisting, such that with one deb installation you can use this driver instead.

LICENSE = "CLOSED"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = "\
    file://CMakeLists.txt \
    file://main.cpp \
    file://Gpio/Src/Gpio.cpp \
    file://Gpio/Inc/Gpio.hpp \
    "

S = "${WORKDIR}"

do_rm_work[noexec] = "1"

DEPENDS += " fmt libgpiod"

inherit cmake pkgconfig

do_install() {
    install -d ${D}/home/root
    install -m 0755 ${B}/axi-gpio ${D}/home/root/
}

FILES:${PN} += "/home/root/axi-gpio"

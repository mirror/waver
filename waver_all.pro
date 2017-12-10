TEMPLATE = subdirs
SUBDIRS +=                                 \
   waver/waver.pro                         \
   wp_equalizer/wp_equalizer.pro           \
   wp_localsource/wp_localsource.pro       \
   wp_radiosource/wp_radiosource.pro       \
   wp_soundoutput/wp_soundoutput.pro       \
   wp_taglibinfo/wp_taglibinfo.pro         \
   wp_albumart/wp_albumart.pro             \
   wp_fmasource

unix:!android {
   SUBDIRS += wp_genericdecoder/wp_genericdecoder.pro
}

windows {
   SUBDIRS += wp_mpg123decoder/wp_mpg123decoder.pro
}

TEMPLATE = subdirs
SUBDIRS +=                                 \
   waver/waver.pro                         \
   wp_equalizer/wp_equalizer.pro           \
   wp_localsource/wp_localsource.pro       \
   wp_radiosource/wp_radiosource.pro       \
   wp_soundoutput/wp_soundoutput.pro       \
   wp_albumart/wp_albumart.pro             \
   wp_sftpsource                           \
   wp_acoustid                             \
   wp_rmsmeter

# FMA seems to have trouble with their API since they became quasi-commercialzed
# disabling FMA source for now, still deciding what to do, but leaning towards removing it completely
#wp_fmasource

unix:!android {
    SUBDIRS +=                                  \
        wp_genericdecoder/wp_genericdecoder.pro \
        wp_mpg123decoder/wp_mpg123decoder.pro
}

windows {
    SUBDIRS += wp_mpg123decoder/wp_mpg123decoder.pro
}

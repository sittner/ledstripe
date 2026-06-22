idf_component_register(
    SRCS "main.c" "stripe.c" "font.c"
    INCLUDE_DIRS "."
    REQUIRES driver led_strip
)

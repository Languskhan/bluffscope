# The screens are shared with the PC simulator (../../ui).
idf_component_register(
    SRCS "main.c" "../../ui/bs_model.c" "../../ui/bs_ui.c"
    INCLUDE_DIRS "." "../../ui")

@echo off

setLocal

set "listings_path=listings\"
set "listings=listing_0037_single_register_mov listing_0038_many_register_mov listing_0039_more_movs listing_0041_add_sub_cmp_jnz listing_0043_immediate_movs listing_0044_register_movs listing_0046_add_sub_cmp listing_0048_ip_register"

for %%v in (%listings%) do (
    nasm "%listings_path%%%v.asm"
    build\main.exe  %listings_path%%%v > result.asm
    nasm result.asm
    fc result %listings_path%%%v
)

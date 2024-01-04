@echo off

setLocal

set "listings_path=listings\"
set "listings=listing_0037_single_register_mov listing_0038_many_register_mov listing_0039_more_movs listing_0041_add_sub_cmp_jnz"

for %%v in (%listings%) do (
    nasm "%listings_path%%%v.asm"
    build\main.exe  %listings_path%%%v > result.asm
    nasm result.asm
    fc result %listings_path%%%v
)

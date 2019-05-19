set tabstop=4
set softtabstop=4
set shiftwidth=4
set noexpandtab
augroup project
    autocmd!
    autocmd BufRead,BufNewFile *.h,*.c set filetype=c.doxygen
augroup END
let &path.="src/include,/usr/include/AL,"

"YCM default config for C family
let g:ycm_global_ycm_extra_conf = "~/scripts/.ycm_extra_config.py"

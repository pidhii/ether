let gtk_options = system("pkg-config gtk+-3.0 --cflags")[0:-2]
let g:ale_c_gcc_options = g:ale_c_gcc_options . " " . gtk_options

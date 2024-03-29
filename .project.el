(c++-mode . ((filetypes . (cpp-project))
             (includes . ("accounts"
                          "cmdsender"
                          "cmdserver"
                          "countsmswords"
                          "etagswrapper"
                          "imagediff"
                          "list"
                          "uri2uri"))
             (flags . ("-xc++"
                       "-std=c++11"))
             (warnings . ("all"
                          "extra"
                          "everything"))
             (packages . ("rdlib-0.1"))
             (sources . ("src"
                         "../rdlib/include"
                         "../rdlib/src"))
             (buildcmd . "make -f makefile")))

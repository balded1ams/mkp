# mkp
mkp is a software to manage templates
# installation

install : `make -B`
to access it from everywhere add it to the binary :
```bash
 $ sudo chmod +x mkp
 $ sudo ln -s /path/to/binary /usr/local/bin/mkp
```

add your project template to `.local/template` the name of the templates should be the name of the language 
if you don't want to add yours or don't have any, you can get mine through the software: `mkp --init`

# code
## comment
the function description comment where generated by chat-gpt
## test
to run the unit test simply compile whith `make test`
then run the executable

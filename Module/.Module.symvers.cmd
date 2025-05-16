cmd_/home/usm/usm/Module/Module.symvers := sed 's/\.ko$$/\.o/' /home/usm/usm/Module/modules.order | scripts/mod/modpost -m -a  -o /home/usm/usm/Module/Module.symvers -e -i Module.symvers   -T -

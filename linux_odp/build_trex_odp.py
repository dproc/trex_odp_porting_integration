#!/usr/bin/python
# -*- coding: utf-8 -*-

# Hanoh Haim
# wenxian.li@linaro.org

import pexpect
import re
import sys 


class CEnum :
    EOK = 0
    ERR = 1

class COptions:
    has_pull_dpdk      = False
    has_build_dpdk     = False
    has_pull_odp       = False
    has_build_odp      = False
    has_build_trex     = False
    has_mkdir_dpdk     = False
    has_test_odp       = False
    base_dir           = ''
    bash               = pexpect.spawn("bash") 
    cli_idx            = 0

    def cmd_exec(self, cli, is_print=True):
        print("-----cli:%d"%(self.cli_idx))
        self.cli_idx = self.cli_idx+1
        print(cli)
        output = ''
        cli_res = ''
        self.bash.sendline(r""+cli)
        self.bash.expect([".",pexpect.EOF], timeout=300000)
        while self.bash.after!=pexpect.EOF and self.bash.after not in "#" :
            if self.bash.after in '\n\r' and is_print==True:
                print(output)
                output = ''
            else :
                output = output+self.bash.before+self.bash.after
            cli_res = cli_res+self.bash.before+self.bash.after
            self.bash.expect([".",pexpect.EOF], timeout=300000)
        print(self.bash.after)
        return cli_res

    def __init__(self) :
        if self.parse_opt(sys.argv) != CEnum.EOK :
            sys.exit()
        self.start_bash()
        cli_raw = self.cmd_exec("pwd")
        lines = cli_raw.split('\n')
        res = re.findall(r'(.*)\r',lines[1])
        self.base_dir = res[0]
        self.mkdir_dpdk() 
        self.base_dir = self.base_dir+"/build_odp/"
        print("Based Dir is: "+self.base_dir)

    def start_bash(self): 
        self.bash = pexpect.spawn("bash")
        self.bash.expect(["#",pexpect.EOF])
        if self.base_dir != '':
            self.cmd_exec("cd "+self.base_dir)
    
    def mkdir_dpdk(self) :
        self.start_bash()
        self.cmd_exec("mkdir build_odp")
        self.has_mkdir_dpdk = True

    def pull_dpdk(self) :
        print("----------")
        print("pull_dpdk starts")
        self.start_bash()
        self.cmd_exec(r'git clone http://92.243.14.124/git/dpdk dpdk_src')
        self.cmd_exec('cd dpdk_src')
        self.cmd_exec(r'git checkout -b 2.2.0-rc1 tags/v2.2.0-rc1')
        print("pull_dpdk finishes")
        print("----------")

    def build_dpdk(self) :
        print("----------")
        print("build_dpdk starts")
        self.start_bash()
        self.cmd_exec(r'cd dpdk_src')
        self.cmd_exec(r'make config T=x86_64-native-linuxapp-gcc O=x86_64-native-linuxapp-gcc')
        self.cmd_exec(r'cd ./x86_64-native-linuxapp-gcc')
        self.cmd_exec(r"sed -ri 's,(CONFIG_RTE_BUILD_COMBINE_LIBS=).*,\1y,' .config")
        self.cmd_exec(r"sed -ri 's,(CONFIG_RTE_LIBRTE_PMD_PCAP=).*,\1y,' .config")
        self.cmd_exec(r"sed -ri 's,(CONFIG_RTE_LIBRTE_IXGBE_ALLOW_UNSUPPORTED_SFP=).*,\1y,' .config")
        self.cmd_exec("cd ..")
        self.cmd_exec(r'make install T=x86_64-native-linuxapp-gcc EXTRA_CFLAGS="-fPIC"')
        print("build_dpdk finishes")
        print("----------")

    def pull_odp_dpdk(self) :
        print("----------")
        print("pull_odp_dpdk starts")
        self.start_bash()
        self.cmd_exec(r'git clone http://git.linaro.org/lng/odp-dpdk.git odp-dpdk')
        print("pull_odp_dpdk finishes")
        print("----------")

    def odp_test(self) :
        print("----------")
        print("odp_test starts")
        self.start_bash()
        self.cmd_exec('cd odp-dpdk')
        self.cmd_exec(r'./bootstrap')
        self.cmd_exec(r'./configure --with-platform=linux-dpdk --with-sdk-install-path='+self.base_dir+'dpdk_src/x86_64-native-linuxapp-gcc'+ " --prefix="+self.base_dir+"odp-dpdk/build "+"--enable-test-vald --enable-valgrind --enable-test-perf")
        self.cmd_exec("make check")
        print("odp_test finishes")
        print("----------")


    def build_odp_dpdk(self) :
        print("----------")
        print("build_odp_dpdk starts")
        self.start_bash()
        self.cmd_exec('cd odp-dpdk')
        self.cmd_exec(r'./bootstrap')
        self.cmd_exec(r'./configure --with-platform=linux-dpdk --with-sdk-install-path='+self.base_dir+'dpdk_src/x86_64-native-linuxapp-gcc'+ " --prefix="+self.base_dir+"odp-dpdk/build")
        self.cmd_exec('make')
        self.cmd_exec("make install")
        #cp alias with "cp -i", use \cp to overright it
        self.cmd_exec(r"mkdir ../../../external_libs/odp")
        self.cmd_exec(r"\cp build/lib/libodp* ../../../external_libs/odp/")
        print("build_odp_dpdk finishes")
        print("----------")

    def build_trex(self) :
        print("----------")
        print("build_trex starts")
        self.start_bash()
        self.cmd_exec('cd ..')
        self.cmd_exec(r'./b configure')
        self.cmd_exec(r'./b build')
        print("----------")
        print("build_trex ends")
 

    def run(self) :
        if self.has_pull_dpdk==True :
            self.pull_dpdk()
        if self.has_build_dpdk==True :
            self.build_dpdk()
        if self.has_pull_odp==True:
            self.pull_odp_dpdk()
        if self.has_build_odp==True :
            self.build_odp_dpdk()
        if self.has_test_odp==True :
            self.odp_test()
        if self.has_build_trex==True :
            self.build_trex()

    def parse_opt(self, argv) :
        print("# of argv:%d"%(len(argv)))
        for i in range(1,len(argv)):
            arg = argv[i]
            if arg=="pull-dpdk" :
                self.has_pull_dpdk = True
            elif arg=="build-dpdk" :
                self.has_build_dpdk = True
            elif arg=="pull-odp" :
                self.has_pull_odp = True
            elif arg=="build-odp" :
                self.has_build_odp = True
            elif arg=="test-odp" :
                self.has_test_odp = True
            elif arg=="build-trex" :
                self.has_build_trex = True
            elif arg=="all" :
                self.has_pull_dpdk  = True
                self.has_build_dpdk = True
                self.has_pull_odp   = True
                self.has_build_odp  = True
                self.has_build_trex = True
                self.has_test_odp = True
            else :
                print("unsupported arg: "+arg)
                print("Supported arg includes:")
                print("pull-dpdk  :  clone dpdk source code from git server")
                print("pull-odp   :  clone odp-dpdk source code from git server")
                print("build-dpdk :  build dpdk package")
                print("build-odp  :  build odp-dpdk package")
                print("test-odp   :  run odp-dpdk test validation")
                print("build-trex :  build trex package")
                print("all        :  all jobs above")
                return CEnum.ERR
        return CEnum.EOK

def main():
    opt = COptions()
    opt.run()


main()

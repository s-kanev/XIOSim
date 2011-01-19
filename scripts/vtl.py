#! /usr/bin/python
import shlex, subprocess
import os

import counters
import spec

#Generic interface to build command lines for vtune
class VtlDriver(object):

    def __init__(self):
        self.cmd = "vtl "
        self.app = ""
        self.path = ""

    def AddSampling(self, counters):
        self.cmd += "activity "
        self.cmd += "-c sampling "
        self.cmd += "-o \" -ec "

        if counters == None or counters == []:
            raise Exception("Add at least one counter to track!")

        for counter in counters:
            self.cmd += counter()        

        self.cmd += "\" "
        return self.cmd

    def AddApp(self, app, params, directory):
        if directory != "":
            app = os.path.join(directory, app)

        self.cmd += "-app %s" % app

        if params != "":
            self.cmd += ",\"%s\"" % params
        else:
            self.cmd += ","

        if directory != "":
            self.cmd += ",\"%s\"" % directory

        self.cmd += " "
        return self.cmd

    def AddRun(self):
        self.cmd += "run "
        return self.cmd

    def AddView(self):
        self.cmd += "view "
        return self.cmd

    def Exec(self, directory=".", redirin="", redirout="", redirerr=""):
        print "Executing: %s" % self.cmd

        if redirin != "":
            stdin = open(os.path.join(directory, redirin), "r")
        else:
            stdin = None

        if redirout != "":
            stdout = open(os.path.join(directory, redirout), "w")
        else:
            stdout = None

        if redirerr != "":
            stderr = open(os.path.join(directory, redirerr), "w")
        else:
            stderr = None

        child = subprocess.Popen(shlex.split(self.cmd), stdin=stdin, stdout=stdout, stderr=stderr, close_fds=True)
        retcode = child.wait()

        if retcode == 0:
            print "Child completed successfully"
        else:
            print "Child failed! Error code: %d" % retcode


def RunSPEC():
    for run in spec.runs:
        vtl = VtlDriver() 
        vtl.AddSampling([counters.CountCLK, counters.CountINST])
        vtl.AddApp(run.executable, run.args, run.directory)

        vtl.AddRun()
        vtl.AddView()
        vtl.Exec(run.directory, run.input, run.output, run.error)


def GetSpecCPI():
    #hardcoded cpi col
    col = 4
    for run in spec.runs:
        runoutput = os.path.join(run.directory, run.output)
        for line in open(runoutput):
            if run.executable in line:
                cpioutput = os.path.splitext(runoutput)[0] + ".cpi"
                outf = open(cpioutput, "w")
                outf.write("%s\n" % line.split()[col])
                outf.close()
                nameoutput = os.path.splitext(runoutput)[0] + ".name"
                outf = open(nameoutput, "w")
                outf.write("%s\n" % run.name)
                outf.close()
                break

if __name__ == "__main__":
    GetSpecCPI()


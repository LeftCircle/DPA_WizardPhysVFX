#!/usr/bin/python

import os
import sys
import getpass

possible_assignments = ['1', '2', '3', 'starterkit']

username = getpass.getuser()
assignment = "UNKNOWN"

while possible_assignments.count(assignment) < 1:
    print "Assignment \'" + assignment + "\' is NOT a valid assignment number."
    print "Valid assignments are:"
    for a in possible_assignments:
        print "\t" + str(a)
    print "\n"
    assignment = raw_input( "Enter assignment: ")

zipfilename = username + "_" + assignment + ".zip"
cmd = "cd ..;zip -r " + zipfilename + " pbalite  -x \"pbalite/.git/*\""
os.system(cmd)
print "Zip file \'" + zipfilename + "\' has been created."


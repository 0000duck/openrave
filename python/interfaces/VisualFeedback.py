# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
from __future__ import with_statement # for python 2.5
__author__ = 'Rosen Diankov'
__copyright__ = 'Copyright (C) 2009-2010 Rosen Diankov (rosen.diankov@gmail.com)'
__license__ = 'Apache License, Version 2.0'
import os, time, pickle
import numpy # nice to be able to explicitly call some functions
from numpy import *
from optparse import OptionParser
from openravepy import *
from copy import copy as shallowcopy

class VisualFeedback:
    def __init__(self,robot,maxvelmult=None):
        env = robot.GetEnv()
        self.prob = env.CreateProblem('VisualFeedback')
        self.robot = robot
        self.args = self.robot.GetName()
        if maxvelmult is not None:
            self.args += ' maxvelmult %f '%maxvelmult
        if env.LoadProblem(self.prob,self.args) != 0:
            raise ValueError('problem failed to initialize')
    def  __del__(self):
        self.prob.GetEnv().RemoveProblem(self.prob)
    def clone(self,envother):
        clone = shallowcopy(self)
        clone.prob = envother.CreateProblem('VisualFeedback')
        clone.robot = envother.GetRobot(self.robot.GetName())
        if envother.LoadProblem(clone.prob,clone.args) != 0:
            raise ValueError('problem failed to initialize')
        return clone
    def SetCameraAndTarget(self,sensorindex=None,sensorname=None,manipname=None,convexdata=None,sensorrobot=None,target=None):
        """.. interface-command:: VisualFeedback SetCameraAndTarget
        """
        cmd = 'SetCameraAndTarget '
        if target is not None:
            cmd += 'target %s '%target.GetName()
        if sensorrobot is not None:
            cmd += 'sensorrobot %s '%sensorrobot.GetName()
        if sensorindex is not None:
            cmd += 'sensorindex %d '%sensorindex
        if sensorname is not None:
            cmd += 'sensorname %s '%sensorname
        if manipname is not None:
            cmd += 'manipname %s '%manipname
        if convexdata is not None:
            cmd += 'convexdata %d '%len(convexdata)
            for f in reshape(convexdata,len(convexdata)*2):
                cmd += str(f) + ' '
        res = self.prob.SendCommand(cmd)
        if res is None:
            raise planning_error()
        return res
    def ProcessVisibilityExtents(self,localtargetcenter=None,numrolls=None,transforms=None,extents=None,sphere=None,conedirangles=None):
        """.. interface-command:: VisualFeedback ProcessVisibilityExtents
        """
        cmd = 'ProcessVisibilityExtents '
        if localtargetcenter is not None:
            cmd += 'localtargetcenter %f %f %f '%(localtargetcenter[0],localtargetcenter[1],localtargetcenter[2])
        if numrolls is not None:
            cmd += 'numrolls %d '%numrolls
        if transforms is not None:
            cmd += 'transforms %d '%len(transforms)
            for f in reshape(transforms,len(transforms)*7):
                cmd += str(f) + ' '
        if extents is not None:
            cmd += 'extents %d '%len(extents)
            for f in reshape(extents,len(extents)*3):
                cmd += str(f) + ' '
        if sphere is not None:
            cmd += 'sphere %d %d '%(sphere[0],len(sphere)-1)
            for s in sphere[1:]:
                cmd += '%f '%s
        if conedirangles is not None:
            for conedirangle in conedirangles:
                cmd += 'conedirangle %f %f %f '%(conedirangle[0],conedirangle[1],conedirangle[2])
        res = self.prob.SendCommand(cmd)
        if res is None:
            raise planning_error()
        visibilitytransforms = array([float(s) for s in res.split()],float)
        return reshape(visibilitytransforms,(len(visibilitytransforms)/7,7))
    def SetCameraTransforms(self,transforms,mindist=None):
        """.. interface-command:: VisualFeedback SetCameraTransforms
        """
        cmd = 'SetCameraTransforms transforms %d '%len(transforms)
        for f in reshape(transforms,len(transforms)*7):
            cmd += str(f) + ' '
        if mindist is not None:
            cmd += 'mindist %f '%(mindist)
        res = self.prob.SendCommand(cmd)
        if res is None:
            raise planning_error()
        return res
    def ComputeVisibility(self):
        """.. interface-command:: VisualFeedback ComputeVisibility
        """
        cmd = 'ComputeVisibility '
        res = self.prob.SendCommand(cmd)
        if res is None:
            raise planning_error()
        return int(res)
    def ComputeVisibleConfiguration(self,pose):
        """.. interface-command:: VisualFeedback ComputeVisibleConfiguration
        """
        cmd = 'ComputeVisibleConfiguration '
        cmd += 'pose '
        for i in range(7):
            cmd += str(pose[i]) + ' '
        res = self.prob.SendCommand(cmd)
        if res is None:
            raise planning_error()
        return array([float(s) for s in res.split()])
    def SampleVisibilityGoal(self,numsamples=None):
        """.. interface-command:: VisualFeedback SampleVisibilityGoal
        """
        cmd = 'SampleVisibilityGoal '
        if numsamples is not None:
            cmd += 'numsamples %d '%numsamples
        res = self.prob.SendCommand(cmd)
        if res is None:
            raise planning_error()
        samples = [float(s) for s in res.split()]
        returnedsamples = int(samples[0])
        return reshape(array(samples[1:],float),(returnedsamples,(len(samples)-1)/returnedsamples))
    def MoveToObserveTarget(self,affinedofs=None,smoothpath=None,planner=None,sampleprob=None,maxiter=None,execute=None,outputtraj=None):
        """.. interface-command:: VisualFeedback MoveToObserveTarget
        """
        cmd = 'MoveToObserveTarget '
        if affinedofs is not None:
            cmd += 'affinedofs %d '%affinedofs
        if smoothpath is not None:
            cmd += 'smoothpath %d '%smoothpath
        if planner is not None:
            cmd += 'planner %s '%planner
        if sampleprob is not None:
            cmd += 'sampleprob %f '%sampleprob
        if execute is not None:
            cmd += 'execute %d '%execute
        if outputtraj is not None and outputtraj:
            cmd += 'outputtraj '
        if maxiter is not None:
            cmd += 'maxiter %d '%maxiter
        res = self.prob.SendCommand(cmd)
        if res is None:
            raise planning_error()
        return res
    def VisualFeedbackGrasping(self,graspset,usevisibility=None,planner=None,graspdistthresh=None,visgraspthresh=None,numgradientsamples=None,maxiter=None,execute=None,outputtraj=None):
        """.. interface-command:: VisualFeedback VisualFeedbackGrasping
        """
        cmd = 'VisualFeedbackGrasping '
        if graspset is not None:
            cmd += 'graspset %d '%len(graspset)
            for f in reshape(graspset,size(graspset)):
                cmd += str(f) + ' '
        if usevisibility is not None:
            cmd += 'usevisibility %d '%usevisibility
        if planner is not None:
            cmd += 'planner %s '%planner
        if graspdistthresh is not None:
            cmd += 'graspdistthresh %f '%graspdistthresh
        if visgraspthresh is not None:
            cmd += 'visgraspthresh %f '%visgraspthresh
        if numgradientsamples is not None:
            cmd += 'numgradientsamples %d '%numgradientsamples
        if execute is not None:
            cmd += 'execute %d '%execute
        if outputtraj is not None and outputtraj:
            cmd += 'outputtraj '
        if maxiter is not None:
            cmd += 'maxiter %d '%maxiter
        res = self.prob.SendCommand(cmd)
        if res is None:
            raise planning_error()
        return res

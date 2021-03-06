/*
  The RealityGrid Steering Library VTK Data Reader

  Copyright (c) 2002-2009, University of Manchester, United Kingdom.
  All rights reserved.

  This software is produced by Research Computing Services, University
  of Manchester as part of the RealityGrid project and associated
  follow on projects, funded by the EPSRC under grants GR/R67699/01,
  GR/R67699/02, GR/T27488/01, EP/C536452/1, EP/D500028/1,
  EP/F00561X/1.

  LICENCE TERMS

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of The University of Manchester nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  Author: Robert Haines
 */

#include <iostream>

#include "vtkRealityGridDataReader.h"
#include "vtkRealityGridIOChannel.h"

#include "vtkCallbackCommand.h"
#include "vtkCommand.h"
#include "vtkRenderWindowInteractor.h"

#include "ReG_Steer_Appside.h"

vtkCxxRevisionMacro(vtkRealityGridDataReader, "Revision: 0.01");

vtkRealityGridDataReader* vtkRealityGridDataReader::_instance = NULL;

vtkRealityGridDataReader* vtkRealityGridDataReader::New() {
  if(_instance == NULL)
    _instance = new vtkRealityGridDataReader();

  return _instance;
}

vtkRealityGridDataReader* vtkRealityGridDataReader::GetInstance() {
  return _instance;
}

// instantiate default
vtkRealityGridDataReader::vtkRealityGridDataReader() {
  for(int i = 0; i < REG_INITIAL_NUM_IOTYPES; i++) {
    this->io_channels[i] = NULL;
  }
  this->interactor = NULL;
  this->InitializeRealityGrid();
}

vtkRealityGridDataReader::~vtkRealityGridDataReader() {
  this->FinalizeRealityGrid();
  _instance = NULL;
}

void vtkRealityGridDataReader::PrintSelf(ostream& os, vtkIndent indent) {
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Interactor: " << (interactor == NULL ? "not set" : "set") << "\n";
  os << indent << "IO Channels: ";
  if(num_io_channels > 0) {
    os << num_io_channels << std::endl;
    for(int i = 0; i < num_io_channels; i++) {
      io_channels[i]->PrintSelf(os, indent.GetNextIndent());
    }
  }
  else {
      os << "(none)\n";
  }
  os << indent << "Loop number: " << this->loop_number << "\n";
}

void vtkRealityGridDataReader::SetInteractor(vtkRenderWindowInteractor* i) {
  this->interactor = i;

  vtkCallbackCommand* timer_callback = vtkCallbackCommand::New();
  timer_callback->SetCallback(_poll);
  timer_callback->SetClientData((void*) this);
  this->interactor->AddObserver(vtkCommand::TimerEvent, timer_callback);
  this->interactor->CreateTimer(VTKI_TIMER_FIRST);
}

// Initialize the RealityGrid steering library
void vtkRealityGridDataReader::InitializeRealityGrid() {

  // allocate memory
  changedParamLabels = Alloc_string_array(REG_MAX_STRING_LENGTH,
					  REG_MAX_NUM_STR_PARAMS);
  recvdCmdParams = Alloc_string_array(REG_MAX_STRING_LENGTH,
				      REG_MAX_NUM_STR_CMDS);

  // initialise steering library
  Steering_enable(REG_TRUE);
  Steering_initialize((char*) "RealityGrid VTK Data Reader", 0, NULL);

  loop_number = 0;
  num_io_channels = 0;
}

// Call Steering_control and do required steering ops
bool vtkRealityGridDataReader::PollRealityGrid() {
  int status;
  bool updated = false;

  status = Steering_control(loop_number,
			    &numParamsChanged,
			    changedParamLabels,
			    &numRecvdCmds,
			    recvdCmds,
			    recvdCmdParams);

  if(status != REG_SUCCESS) {
    std::cerr << "Call to Steering_control failed...\n";
    return false;
  }

  // deal with steering commands
  for(int i = 0; i < numRecvdCmds; i++) {

    // update the IO channels
    for(int j = 0; j < num_io_channels; j++) {
      if(recvdCmds[i] == io_channels[j]->GetHandle())
	updated = io_channels[j]->Update(loop_number);
    }
  }

  loop_number++;

  return updated;
}

void vtkRealityGridDataReader::FinalizeRealityGrid() {
  // clean up steering library
  Steering_finalize();
}

// Register an IO channel
// TODO: check bounds of io_handles[]
void vtkRealityGridDataReader::RegisterIOChannel(vtkRealityGridIOChannel* c, int freq) {
  int status;
  int iohandle;

  status = Register_IOType(c->GetName(), c->GetIODirection(), freq, &iohandle);
  if(status == REG_SUCCESS) {
    c->SetHandle(iohandle);
    io_channels[num_io_channels++] = c;
  }
}

// friend method for VTK polling loop callback
// TODO: Might need some throttling at some point?
void _poll(vtkObject* obj, unsigned long eid, void* cd, void* calld) {
  // get VTK interactor
  vtkRenderWindowInteractor* i = vtkRenderWindowInteractor::SafeDownCast(obj);

  // get vtkRealityGridDataReader instance
  vtkRealityGridDataReader* reader = (vtkRealityGridDataReader*) cd;

  // do ReG stuff and re-render if necessary
  if(reader->PollRealityGrid()) {

    // re-render
    i->Render();
  }

  // reset timer
  i->CreateTimer(VTKI_TIMER_UPDATE);
}

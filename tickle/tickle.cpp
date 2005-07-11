/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005 Gunnar Beutner                                           *
 *                                                                             *
 * This program is free software; you can redistribute it and/or               *
 * modify it under the terms of the GNU General Public License                 *
 * as published by the Free Software Foundation; either version 2              *
 * of the License, or (at your option) any later version.                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *******************************************************************************/

#include "StdAfx.h"
#include "../ModuleFar.h"
#include "../BouncerCore.h"
#include "../SocketEvents.h"
#include "../DnsEvents.h"
#include "../Connection.h"
#include "../IRCConnection.h"
#include "../ClientConnection.h"
#include "../Channel.h"
#include "../BouncerUser.h"
#include "../BouncerConfig.h"
#include "../Hashtable.h"
#include "TclSocket.h"
#include "TclClientSocket.h"
#include "tickle.h"
#include "tickleProcs.h"

#define FREEZEFIX

class CTclSupport;

CBouncerCore* g_Bouncer;
Tcl_Interp* g_Interp;
CTclSupport* g_Tcl;
bool g_Ret;
bool g_NoticeUser;

#ifdef _WIN32
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 ) {
    return TRUE;
}
#else
int main(int argc, char* argv[]) { return 0; }
#endif

int strcmpi(const char* a, const char* b) {
	while (*a && *b) {
		if (tolower(*a) != tolower(*b))
			return 1;

		++a;
		++b;
	}

	if (*a == *b)
		return 0;
	else
		return 1;
}

int Tcl_AppInit(Tcl_Interp *interp) {
	if (Tcl_Init(interp) == TCL_ERROR)
		return TCL_ERROR;

	if (Tcl_ProcInit(interp) == TCL_ERROR)
		return TCL_ERROR;

	return TCL_OK;
}

class CTclSupport : public CModuleFar {
	void Destroy(void) {
		CallBinds(Type_Unload, NULL, 0, NULL);

		Tcl_DeleteInterp(g_Interp);

		int i = 0;

		while (xhash_t<CTclSocket*>* p = g_TclListeners->Iterate(i)) {
			p->Value->Destroy();
		}

		delete g_TclListeners;

		i = 0;

		while (xhash_t<CTclClientSocket*>* p = g_TclClientSockets->Iterate(i)) {
			p->Value->Destroy();
		}

		delete g_TclClientSockets;

		delete this;
	}

	void Init(CBouncerCore* Root) {
		g_Bouncer = Root;

		g_TclListeners = new CHashtable<CTclSocket*, false>;
		g_TclClientSockets = new CHashtable<CTclClientSocket*, false>;

		Tcl_FindExecutable(Root->GetArgV()[0]);

		Tcl_SetSystemEncoding(NULL, "ISO8859-1");

		g_Interp = Tcl_CreateInterp();

		Tcl_SetVar(g_Interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);

		Tcl_AppInit(g_Interp);

		Tcl_EvalFile(g_Interp, "./sbnc.tcl");
	}

	bool InterceptIRCMessage(CIRCConnection* IRC, int argc, const char** argv) {
		g_Ret = true;

		CallBinds(Type_PreScript, NULL, 0, NULL);
		CallBinds(Type_Server, IRC->GetOwningClient()->GetUsername(), argc, argv);
		CallBinds(Type_PostScript, NULL, 0, NULL);

		return g_Ret;
	}

	bool InterceptClientMessage(CClientConnection* Client, int argc, const char** argv) {
		g_Ret = true;

		CBouncerUser* User = Client->GetOwningClient();

		CallBinds(Type_PreScript, NULL, 0, NULL);
		CallBinds(Type_Client, User ? User->GetUsername() : NULL, argc, argv);
		CallBinds(Type_PostScript, NULL, 0, NULL);

		return g_Ret;
	}

	void AttachClient(const char* Client) {
		CallBinds(Type_Attach, Client, 0, NULL);
	}

	void DetachClient(const char* Client) {
		CallBinds(Type_Detach, Client, 0, NULL);
	}

	void ServerDisconnect(const char* Client) {
		CallBinds(Type_SvrDisconnect, Client, 0, NULL);
	}

	void ServerConnect(const char* Client) {
		CallBinds(Type_SvrConnect, Client, 0, NULL);
	}

	void ServerLogon(const char* Client) {
		CallBinds(Type_SvrLogon, Client, 0, NULL);
	}

	void UserLoad(const char* User) {
		CallBinds(Type_UsrLoad, User, 0, NULL);
	}

	void UserCreate(const char* User) {
		CallBinds(Type_UsrCreate, User, 0, NULL);
	}

	void UserDelete(const char* User) {
		CallBinds(Type_UsrDelete, User, 0, NULL);
	}

	void SingleModeChange(CIRCConnection* IRC, const char* Channel, const char* Source, bool Flip, char Mode, const char* Parameter) {
		char ModeC[3];

		ModeC[0] = Flip ? '+' : '-';
		ModeC[1] = Mode;
		ModeC[2] = '\0';

		const char* argv[4] = { Source, Channel, ModeC, Parameter };

		CallBinds(Type_SingleMode, IRC->GetOwningClient()->GetUsername(), Parameter ? 4 : 3, argv);
	}

	const char* Command(const char* Cmd, const char* Parameters) {
		if (strcmpi(Cmd, "tcl:eval") == 0) {
			Tcl_Eval(g_Interp, Parameters);

			Tcl_Obj* Result = Tcl_GetObjResult(g_Interp);

			const char* strResult = Tcl_GetString(Result);

			return strResult;
		}

		return NULL;
	}

	bool InterceptClientCommand(CClientConnection* Client, const char* Subcommand, int argc, const char** argv, bool NoticeUser) {
		CBouncerUser* User = Client->GetOwningClient();

		g_NoticeUser = NoticeUser;

		if (argc > 1 && strcmpi(Subcommand, "tcl") == 0 && User && User->IsAdmin()) {
			setctx(User->GetUsername());

			int Code = Tcl_Eval(g_Interp, argv[1]);

			Tcl_Obj* Result = Tcl_GetObjResult(g_Interp);

			const char* strResult = Tcl_GetString(Result);

			if (Code == TCL_ERROR) {
				if (NoticeUser)
					User->RealNotice("An error occured in the tcl script:");
				else
					User->Notice("An error occured in the tcl script:");
			}

			if (NoticeUser)
				User->RealNotice(strResult ? (*strResult ? strResult : "empty string.") : "<null>");
			else
				User->Notice(strResult ? (*strResult ? strResult : "empty string.") : "<null>");

			return true;
		}

		g_Ret = true;

		CallBinds(Type_Command, Client->GetOwningClient()->GetUsername(), argc, argv);

		return !g_Ret;
	}
public:
	void RehashInterpreter(void) {
		Tcl_EvalFile(g_Interp, "./sbnc.tcl");
	}

};

extern "C" CModuleFar* bncGetObject(void) {
	g_Tcl = new CTclSupport();
	return (CModuleFar*)g_Tcl;
}

void RehashInterpreter(void) {
	g_Tcl->RehashInterpreter();
}

void CallBinds(binding_type_e type, const char* user, int argc, const char** argv) {
	Tcl_Obj** listv;

	//DebugBreak();

	CBouncerUser* User = g_Bouncer->GetUser(user);

	if (User)
		setctx(user);

	int objc = 3;
	Tcl_Obj* objv[3];

	objv[0] = NULL;

	if (user) {
		objv[1] = Tcl_NewStringObj(user ? user : "", user ? strlen(user) : 0);
		Tcl_IncrRefCount(objv[1]);
	} else {
		objv[1] = NULL;
		objc = 1;
	}

	if (argc) {
		listv = (Tcl_Obj**)malloc(sizeof(Tcl_Obj*) * argc);

		for (int a = 0; a < argc; a++) {
			listv[a] = Tcl_NewStringObj(argv[a], strlen(argv[a]));
			Tcl_IncrRefCount(listv[a]);
		}

		objv[2] = Tcl_NewListObj(argc, listv);
		Tcl_IncrRefCount(objv[2]);
	} else {
		objv[2] = NULL;

		if (objc > 2)
			objc = 2;
	}

	for (int i = 0; i < g_BindCount; i++) {
		if (g_Binds[i].valid && g_Binds[i].type == type) {
			objv[0] = Tcl_NewStringObj(g_Binds[i].proc, strlen(g_Binds[i].proc));
			Tcl_IncrRefCount(objv[0]);

			Tcl_EvalObjv(g_Interp, objc, objv, TCL_EVAL_GLOBAL);

			Tcl_DecrRefCount(objv[0]);
		}
	}

	if (user)
		Tcl_DecrRefCount(objv[1]);

	if (argc) {
		for (int a = 0; a < argc; a++) {
			Tcl_DecrRefCount(listv[a]);
		}

		free(listv);

		Tcl_DecrRefCount(objv[2]);
	}
}

void SetLatchedReturnValue(bool Ret) {
	g_Ret = Ret;
}

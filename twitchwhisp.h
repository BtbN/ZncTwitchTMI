#pragma once

#include <znc/Modules.h>

class TwitchGroupChat : public CModule
{
	public:
	MODCONSTRUCTOR(TwitchGroupChat) {}
	virtual ~TwitchGroupChat();

	bool OnLoad(const CString &sArgsi, CString &sMessage) override;
	void OnIRCConnected() override;

	bool OnServerCapAvailable(const CString &sCap) override;

	CModule::EModRet OnRawMessage(CMessage &msg) override;
	CModule::EModRet OnUserTextMessage(CTextMessage &msg) override;

	private:
	CIRCSock *GetTwitchNetwork();
};


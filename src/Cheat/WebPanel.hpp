#pragma once
#include <string>

// Painel web local: sobe um servidor HTTP na rede (porta WEBPANEL_PORT)
// para controlar todas as features do cheat pelo navegador — do mesmo PC
// ou de qualquer celular/PC na mesma rede (http://IP_DO_PC:8080).

namespace WebPanel
{
	void Start( );
	void Stop( );
	bool IsRunning( );

	// URL de acesso pro painel (IP local + porta), ex: http://192.168.0.10:8080
	std::string GetWebUrl( );
}
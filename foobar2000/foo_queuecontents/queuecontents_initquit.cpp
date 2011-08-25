#include "stdafx.h"
#include "config.h"
#include "queuecontents_initquit.h"

void queuecontents_initquit::on_init() 
{	
	TRACK_CALL_TEXT("queuecontents_initquit::on_init");
	if(cfg_playlist_enabled) {
		queuecontents_lock::install_lock();
	}	

	watcher.attach(new playlist_queue_item_watcher());

	//log
#ifdef _DEBUG
	console::formatter() << "QueueContents: DEBUG ENABLED.";
#endif


}

void queuecontents_initquit::on_quit() {
	TRACK_CALL_TEXT("queuecontents_initquit::on_quit");
	queuecontents_lock::uninstall_lock();
	watcher.release();
}


#include "stdafx.h"

// Declaration of your component's version information
// Since foobar2000 v1.0 having at least one of these in your DLL is mandatory to let the troubleshooter tell different versions of your component apart.
// Note that you can declare multiple components within one DLL.
DECLARE_COMPONENT_VERSION(
	// component name
	COMPONENTNAME,
	// component version
	COMPONENTVERSION,
	// about text, use \n to separate multiple lines
	// If you don't want any about text, set this parameter to NULL.
	COMPONENTNAME
	"\n"
	"contributors:\n"
	"Sami Salonen"
	"\n\n"
	"foo_queuecontents enables the user to edit and view queue contents through an ui element. Both the Default User Interface (DUI) and Columns User Interface (CUI/uie) are supported. As a legacy option, queue contents can also be viewed and edited by a special queue playlist, which is updated automatically with queue contents."
	"\n\n"
	"For more information, please see the Wiki page: " COMPONENT_WIKIPAGEURL
	"\n\n"
	"Please use this thread on hydrogenaudio forums to provide feedback, or to report any bugs you might have found: " FORUMURL 
);


// This will prevent users from renaming your component around (important for proper troubleshooter behaviors) or loading multiple instances of it.
VALIDATE_COMPONENT_FILENAME("foo_queuecontents.dll");

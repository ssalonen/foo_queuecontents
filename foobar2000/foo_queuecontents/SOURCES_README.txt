Prerequisite: Visual Studio 2010 (Express does not work.)
1. Extract foo_queuecontents folder under <root>\SDK\foobar2000\
2. Install WTL 8.0 under <WTLRoot>\WTL80. Available from http://sourceforge.net/projects/wtl/
3. Have an environmental variable WTL_HOME pointing to <WTLRoot>\WTL80

As the newest SDK of columns_ui-sdk doesn't work with foobar 1.x SDK, I've modified it (according to various forum posts); the modified CUI SDK is also attached in this zip file. Accompaniying diff file is also included.

All dependencies (ATL helpers, component_client, sdk_helpers, sdk, and uie sdk) should be linked statically to the component (Multi-threaded (/MT) does this, multithreaded dll is dynamic so don't use it!). This means user doesn't have to have c++ runtime libraries, they are bundled with component.
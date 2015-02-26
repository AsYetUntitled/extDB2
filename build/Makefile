COMPILER := g++
CFLAGS := -march=i686 -msse2 -msse3 -fPIC -m32 -O2 -pipe -std=c++0x
STATIC_LIBRARYS := -lPocoCrypto -lPocoUtil -lPocoDataMySQL -lPocoDataSQLite -lPocoData -lPocoNet -lPocoFoundation -lmysqlclient -lboost_filesystem -lboost_random -lboost_regex -lboost_system -lboost_thread -lz
DYNAMIC_LIBRARYS := -ldl -pthread -ltbbmalloc
FILES := src/memory_allocator.cpp src/uniqueid.cpp src/bercon.cpp src/sanitize.cpp src/steam.cpp src/ext.cpp src/protocols/*.cpp 
CUSTOM_MYSQL := -I/usr/local/mysql/include -L/usr/local/mysql/lib

extdb-static:
	$(COMPILER) $(CFLAGS) -shared -static-libstdc++ -static-libgcc -o extDB.so $(FILES) src/main.cpp -Wl,-Bstatic $(STATIC_LIBRARYS) -Wl,-Bdynamic $(DYNAMIC_LIBRARYS) $(CUSTOM_MYSQL)

extdb-static-debug-logging:
	$(COMPILER) $(CFLAGS) -shared -static-libstdc++ -static-libgcc -o extDB.so $(FILES) src/main.cpp -DDEBUG_LOGGING -Wl,-Bstatic $(STATIC_LIBRARYS) -Wl,-Bdynamic $(DYNAMIC_LIBRARYS) $(CUSTOM_MYSQL)

test:
	$(COMPILER) $(CFLAGS) -g -static-libstdc++ -static-libgcc -o extDB-test $(FILES) -DTEST_APP -DTESTING -DDEBUG_LOGGING -Wl,-Bstatic $(STATIC_LIBRARYS) -Wl,-Bdynamic $(DYNAMIC_LIBRARYS) $(CUSTOM_MYSQL)

rcon:
	$(COMPILER) $(CFLAGS) -g -static-libstdc++ -static-libgcc -o extDB-rcon src/rcon.cpp -DRCON_APP -DTESTING -DDEBUG_LOGGING -Wl,-Bstatic $(STATIC_LIBRARYS) -lboost_program_options -Wl,-Bdynamic $(DYNAMIC_LIBRARYS) $(CUSTOM_MYSQL)


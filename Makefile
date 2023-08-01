LCPPOBJ := WebSocketManager.o BinanceOrderBook.o BinanceUserDataStream.o BinanceEndpoint.o
LCPPDEP := $(LCPPOBJ:.o=.d)

CLIBNAME:= binancepp
CLIB	:= lib$(CLIBNAME).so

CXXFLAGS += -I$(WSPPDIR)/include

$(CLIB): $(LCPPOBJ)
	$(CXX) $(CXXFLAGS) -shared -o $@ $^

$(LCPPDEP) $(EDEP): %.d: %.cxx %.h
	@echo "Generating dependency file $@"
	@set -e; rm -f $@
	@$(CXX) -M $(CXXFLAGS) $< > $@.tmp
	@sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.tmp > $@
	@rm -f $@.tmp

include $(LCPPDEP)

$(LCPPOBJ): %.o: %.cxx %.h
	$(CXX) $(CXXFLAGS) -fPIC -c -o $@ $<

clean:
	rm -rf $(LCPPOBJ) $(LCPPDEP)
	rm -rf build

clear: clean
	rm -rf $(CLIB)

#include <seastar/core/sstring.hh>

namespace seastar {
	const char *eptr_to_what (std::exception_ptr eptr) {
	    try {
	      std::rethrow_exception(eptr);
	    } catch (std::exception err) {
			return err.what();
    	}
	
		return nullptr;
	}
};

// namespace std {
// 	template<>
// 	struct formatter<exception_ptr> 
// 	template<class FormatContext>
// 	auto formatter<exception_ptr>::format (exception_ptr eptr, FormatContext& fc) {
// 	  return format_to(fc.out(), "Exception: {}", seastar::eptr_to_what(eptr));
// 	}
// };
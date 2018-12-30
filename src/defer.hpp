#pragma once

namespace detail{
template<typename T>
struct ScopeGuard{
	T lambda;

	ScopeGuard(T t) 
		: lambda(std::move(t)){}

	~ScopeGuard(){
		lambda();
	}

	ScopeGuard() = delete;
	ScopeGuard(const ScopeGuard&) = delete;
	ScopeGuard& operator=(const ScopeGuard&) = delete;
	ScopeGuard(ScopeGuard&& rhs)
		: lambda(std::move(rhs.labmda)){}
};

enum class OnScopeGuardExit {};

template<typename T>
ScopeGuard<T> operator+(OnScopeGuardExit, T&& t){
	return ScopeGuard<T>(std::forward(t));
}

}

#define CONCATENATE_IMPL(x,y) x##y
#define CONCATENATE(x,y) CONCATENATE_IMPL(x,y)

#define defer \
	const auto& CONCATENATE(__defer_, __LINE__) = ::detail::OnScopeGuardExit() + [&]()


module;
#include <boost/variant.hpp>
#include <string>
#include <type_traits>
#include <variant>
export module ivm.value:variant;
import :date;
import :dictionary;
import :primitive;
import :tag;
import :visit;

namespace ivm::value {

// Specialize in order to disable `std::variant` visitor
export template <class... Types>
struct is_variant {
		constexpr static bool value = true;
};

template <class... Types>
constexpr bool is_variant_v = is_variant<Types...>::value;

// Helper which holds a recursive `boost::variant` followed by its alternatives
template <class Variant, class... Types>
struct variant_helper {};

// Helper which extracts recursive variant alternative types
template <class First, class... Rest>
using recursive_variant = boost::variant<boost::detail::variant::recursive_flag<First>, Rest...>;

// Helper which substitutes recursive variant alternative types
template <class Variant, class Type>
using substitute_recursive = typename boost::detail::variant::substitute<Type, Variant, boost::recursive_variant_>::type;

// Covariant `accept` helper for variant-like types
template <class Type, class Result>
struct covariant {};

// Common covariance-aware `accept` for `std::variant` and `boost::variant`
template <class Meta, class Type, class Result>
struct accept<Meta, covariant<Type, Result>> : accept<Meta, Type> {
		using accept<Meta, Type>::accept;

		constexpr auto operator()(auto_tag auto tag, auto&& value) const -> Result
			requires std::is_invocable_v<accept<Meta, Type>, decltype(tag), decltype(value)> {
			const accept<Meta, Type>& parent = *this;
			return parent(tag, std::forward<decltype(value)>(value));
		}
};

template <class Meta, class Type, class Result>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct accept<Meta, covariant<Type, Result>> : accept<Meta, Type> {
		using accept<Meta, Type>::accept;

		constexpr auto operator()(tag_for_t<Type> tag, auto&& value) const -> Result {
			const accept<Meta, Type>& parent = *this;
			return parent(tag, std::forward<decltype(value)>(value));
		}
};

// Accepting a `std::variant` will delegate to each underlying type's acceptor and box the result
// into the variant.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct accept<Meta, std::variant<Types...>>
		: accept<Meta, covariant<Types, std::variant<Types...>>>... {
		using accept<Meta, covariant<Types, std::variant<Types...>>>::operator()...;
};

// Recursive `boost::variant` acceptor
template <class Meta, class Variant, class... Types>
struct accept<Meta, variant_helper<Variant, Types...>>
		: accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>... {
		constexpr accept()
			requires std::conjunction_v<
				std::is_default_constructible<
					accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>...>> {}
		constexpr accept(int dummy, const auto& acceptor) :
				// Pass reference forward for recursive acceptors
				accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>{std::invoke([ & ]() {
					using accept_type = accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>;
					if constexpr (std::is_default_constructible_v<accept_type>) {
						return accept_type{};
					} else {
						return accept_type{dummy, acceptor};
					}
				})}... {}
		using accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>::operator()...;
};

// `accept` entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Rest>
struct accept<Meta, recursive_variant<First, Rest...>>
		: accept<Meta, variant_helper<recursive_variant<First, Rest...>, First, Rest...>> {
		using accept<Meta, variant_helper<recursive_variant<First, Rest...>, First, Rest...>>::accept;
};

// `std::variant` visitor. This used to delegate to the `boost::variant` visitor above, but
// `boost::apply_visitor` isn't constexpr, so we can't use it to test statically. `boost::variant`
// can't delegate to this one either because it handles recursive variants.
template <class... Types>
	requires is_variant_v<Types...>
struct visit<std::variant<Types...>> : visit<Types>... {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return std::visit(
				[ & ]<class Value>(Value&& value) constexpr {
					const visit<std::decay_t<Value>>& visit = *this;
					return visit(std::forward<Value>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// Visiting a `boost::variant` visits the underlying members
template <class Variant, class... Types>
struct visit<variant_helper<Variant, Types...>> : visit<substitute_recursive<Variant, Types>>... {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return boost::apply_visitor(
				[ & ]<class Value>(Value&& value) constexpr {
					const visit<std::decay_t<Value>>& visit = *this;
					return visit(std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// `visit` entry for `boost::make_recursive_variant`
template <class First, class... Rest>
struct visit<recursive_variant<First, Rest...>>
		: visit<variant_helper<recursive_variant<First, Rest...>, First, Rest...>> {
		using visit<variant_helper<recursive_variant<First, Rest...>, First, Rest...>>::visit;
};

} // namespace ivm::value
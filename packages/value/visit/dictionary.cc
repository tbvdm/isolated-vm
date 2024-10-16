module;
#include <boost/variant.hpp>
#include <ranges>
#include <type_traits>
#include <utility>
export module ivm.value:dictionary_visit;
import :dictionary;
import :visit;

namespace ivm::value {

// Look for `boost::recursive_variant_` to determine if this dictionary is recursive
template <class Type>
struct is_recursive {
		constexpr static auto value = false;
};

template <class Type>
constexpr auto is_recursive_v = is_recursive<Type>::value;

template <>
struct is_recursive<boost::recursive_variant_> {
		constexpr static auto value = true;
};

template <template <class...> class Type, class... Types>
struct is_recursive<Type<Types...>> {
		constexpr static auto value = std::disjunction_v<is_recursive<Types>...>;
};

// If the dictionary is not recursive then it will own its own entry acceptor. Otherwise it accepts
// a reference to an existing one.
template <class Type>
struct recursive {};

template <class Meta, class Type>
struct accept<Meta, recursive<Type>> : accept_next<Meta, Type> {
		using accept_next<Meta, Type>::accept_next;
		accept(int /*dummy*/, const auto& /*accept*/) {}
};

template <class Meta, class Type>
	requires is_recursive_v<Type>
struct accept<Meta, recursive<Type>> {
	private:
		using accept_type = accept_next<Meta, Type>;
		const accept_type* accept_;

	public:
		accept() = delete;
		accept(int /*dummy*/, const auto& accept) :
				accept_{&accept} {}

		constexpr auto operator()(auto_tag auto tag, auto&& value) const -> decltype(auto)
			requires std::invocable<accept_type, decltype(tag), decltype(value)> {
			return (*accept_)(tag, std::forward<decltype(value)>(value));
		}
};

// Dictionary's acceptor manages the recursive acceptor for the entry key/value types
template <class Meta, class Tag, class Key, class Value>
struct accept<Meta, dictionary<Tag, Key, Value>> {
	private:
		using accept_key_type = accept<Meta, recursive<Key>>;
		using accept_value_type = accept<Meta, recursive<Value>>;

	public:
		accept()
			requires std::is_default_constructible_v<accept_key_type> &&
			std::is_default_constructible_v<accept_value_type> {}

		accept(int dummy, const auto& accept) :
				accept_key_{dummy, accept},
				accept_value_{dummy, accept} {}

		auto operator()(Tag /*tag*/, auto&& value) const -> dictionary<Tag, Key, Value> {
			return dictionary<Tag, Key, Value>{
				util::into_range(value) |
				std::views::transform([ & ](auto entry) {
					auto&& [ key, value ] = entry;
					return std::pair{
						invoke_visit(std::forward<decltype(key)>(key), accept_key_),
						invoke_visit(std::forward<decltype(value)>(value), accept_value_)
					};
				})
			};
		}

	private:
		accept_key_type accept_key_;
		accept_value_type accept_value_;
};

template <class Tag, class Key, class Value>
struct visit<dictionary<Tag, Key, Value>> {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(Tag{}, std::forward<decltype(value)>(value));
		}
};

} // namespace ivm::value
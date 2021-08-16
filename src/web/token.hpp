#pragma once

#include <map>
#include <memory>
#include <string>
#include <type_traits>

namespace web {

    template<class Handler, typename _Char = char>
    class Token {
    public:
        Token() = default;

        explicit Token(std::basic_string<_Char> ch) : currentMatch(ch) {}

        Token(std::basic_string<_Char> ch, Handler handler)
                : currentMatch(ch), handler(std::move(handler)) {}

        typename std::conditional<std::is_pointer<Handler>::value,
                Handler,
                typename std::add_pointer<Handler>::type>::type findMatch(std::basic_string<_Char> ref) {
            try {
                Token<Handler, _Char> *current = this;
                do {
                    auto other = ref.substr(0, current->currentMatch.length());
                    if (other != current->currentMatch) {
                        return nullptr;
                    }
                    if (other.length() == ref.length()) {
                        return current->getHandler();
                    }

                    ref.erase(0, current->currentMatch.length());

                    auto nextIt = current->nextTokens.find(ref[0]);
                    if (nextIt == current->nextTokens.end()) {
                        return nullptr;
                    }
                    current = &nextIt->second;
                } while (!ref.empty());
            } catch (const std::exception &e) {
                std::cerr << "exception " << e.what() << std::endl;
                return nullptr;
            }
            return nullptr;
        }

        void addSubToken(std::basic_string<_Char> ch, Handler handler);

        const std::basic_string<_Char> &getMatcherString() const {
            return currentMatch;
        }

        template <class T = Handler>
        typename std::enable_if<std::is_function<T>::value, T*>::type getHandler() {
            return handler;
        }

        template <class T = Handler>
        typename std::enable_if<!std::is_pointer<T>::value && !std::is_function<T>::value, T*>::type getHandler() {
            return &handler;
        }

        template <class T = Handler>
        typename std::enable_if<std::is_pointer<T>::value && !std::is_function<T>::value, T>::type getHandler() {
            return handler;
        }

    private:
        std::basic_string<_Char> currentMatch;
        typename std::conditional<std::is_function<Handler>::value,
            typename std::add_pointer<Handler>::type,
            Handler>::type handler;
        std::map<_Char, Token<Handler, _Char>> nextTokens;

        template <class T = Handler>
        typename std::enable_if<!std::is_pointer<T>::value && !std::is_function<T>::value>::type clearHandler() {}

        template <class T = Handler>
        typename std::enable_if<std::is_pointer<T>::value || std::is_function<T>::value>::type clearHandler() {
            handler = nullptr;
        }
    };

    template<class Handler, typename _Char>
    void Token<Handler, _Char>::addSubToken(std::basic_string<_Char> ch, Handler h) {
        std::basic_string<_Char> currentTokenMatch;
        for (typename std::basic_string<_Char>::size_type i = 0U; i < ch.length() && i < currentMatch.length(); ++i) {
            if (ch[i] == currentMatch[i]) {
                currentTokenMatch += ch[i];
            } else break;
        }

        // The requested token matches entirely this token, so add it as a sub
        auto substr = ch.substr(currentTokenMatch.length());
        if (currentTokenMatch.length() == currentMatch.length()) {
            // The requested token is this token, replace the handler
            if (substr.length() == 0) {
                handler = std::move(h);
                return;
            }
            auto firstCh = substr[0];
            auto next = nextTokens.find(firstCh);
            if (next != nextTokens.end()) {
                next->second.addSubToken(substr, std::move(h));
                return;
            }
            nextTokens.insert_or_assign(firstCh, std::move(Token(substr, std::move(h))));
            return;
        }

        // This token shares some parts with the requested token, and currentTokenMatch holds that data
        // We must:
        // 1. Create a new token with the diff and holding the handler of the current token
        // 2. Replace the current token with the shared part and a nullptr handler
        // 3. Add the new token to the current token as a sub token
        // 4. Add the requested token to the new token as a sub token

        Token<Handler, _Char> newToken(currentMatch.substr(currentTokenMatch.length()), std::move(handler));
        clearHandler();
        currentMatch = currentTokenMatch;
        nextTokens.insert_or_assign(newToken.currentMatch[0], std::move(newToken));
        nextTokens.insert_or_assign(substr[0], std::move(Token(substr, std::move(h))));
    }

}

#ifndef __FRACTION_HPP__
#define __FRACTION_HPP__

#include <numeric>
#include <iostream>
#include <cmath>
#include <limits>
#include <concepts>

template<std::integral Int = int>
struct Fraction {
    Int num, denom;
    Fraction(Int num = 0, Int denom = 1): num(num), denom(denom) {
        normalize();
        normalize_sign();
    }
    template<std::integral Int2>
    Fraction(Int2 num, Int2 denom = 1): num(num), denom(denom) {
        normalize();
        normalize_sign();
    }
    template<std::integral Int2>
    Fraction(const Fraction<Int2> &other): num(other.num), denom(other.denom) {}
    Fraction(double d) {
        union {
            double d;
            uint64_t bits;
        } u;
        if (std::fpclassify(d) != FP_NORMAL) {
            num = 0;
            denom = 1;
        } else {
            u.d = d;
            uint64_t mantisa = u.bits & ((1ll << 52) - 1) | 1ll << 52;
            num = mantisa;
            denom = 1;
            int exp = u.bits >> 52 & ((1ll << 11) - 1);
            exp -= 1023;
            if (exp >= 0 && exp < (int) sizeof(Int) * 8 - 1) {
                num <<= exp;
            } else if (exp < 0 && -exp < (int) sizeof(Int) * 8 - 1) {
                denom <<= -exp;
            } else if (exp < 0) {
                num = 0;
            } else {
                num = std::numeric_limits<Int>::max();
                denom = 1;
            }
            if (u.bits >> 63) {
                num = -num;
            }
        }
    }
    void normalize_sign() {
        if (denom < 0) {
            num = -num;
            denom = -denom;
        }
    }
    void normalize() {
        Int g = std::gcd(num, denom);
        num /= g;
        denom /= g;
    }
    Fraction &operator+=(const Fraction &other) {
        Int new_denom = std::lcm(denom, other.denom);
        num *= new_denom / denom;
        num += new_denom / other.denom * other.num;
        denom = new_denom;
        normalize();
        return *this;
    }
    Fraction &operator-=(const Fraction &other) {
        Int new_denom = std::lcm(denom, other.denom);
        num *= new_denom / denom;
        num -= new_denom / other.denom * other.num;
        denom = new_denom;
        normalize();
        return *this;
    }
    Fraction &operator*=(const Fraction &other) {
        int g1 = std::gcd(other.num, denom);
        int g2 = std::gcd(num, other.denom);
        denom /= g1;
        num /= g2;
        num *= other.num / g1;
        denom *= other.denom / g2;
        return *this;
    }
    Fraction &operator/=(const Fraction &other) {
        int g1 = std::gcd(other.num, num);
        int g2 = std::gcd(denom, other.denom);
        denom /= g2;
        num /= g1;
        num *= other.denom / g2;
        denom *= other.num / g1;
        if (denom < 0) {
            num = -num;
            denom = -denom;
        }
        return *this;
    }
    friend Fraction operator+(Fraction x, const Fraction &y) {
        x += y;
        return x;
    }
    friend Fraction operator-(Fraction x, const Fraction &y) {
        x -= y;
        return x;
    }
    friend Fraction operator*(Fraction x, const Fraction &y) {
        x *= y;
        return x;
    }
    friend Fraction operator/(Fraction x, const Fraction &y) {
        x /= y;
        return x;
    }
    auto operator<=>(const Fraction &other) const {
        if (num == 0 || other.num == 0)
            return num <=> other.num;
        Int gn = std::gcd(num, other.num);
        Int gd = std::gcd(denom, other.denom);
        return (num / gn) * (other.denom / gd) <=> (other.num / gn) * (denom / gd);
    }
    bool operator==(const Fraction &other) const {
        return num == other.num && denom == other.denom;
    }
    Fraction operator+() const {
        return *this;
    }
    Fraction operator-() const {
        return Fraction(-num, denom);
    }
};

template<class Int>
inline std::ostream &operator<<(std::ostream &out, const Fraction<Int> &x) {
    return out << x.num << "/" << x.denom;
}

namespace std {

template<class Int>
[[nodiscard]] inline Fraction<Int> abs(Fraction<Int> f) {
    f.num = std::abs(f.num);
    return f;
}

template<class Int>
[[nodiscard]] inline Int floor(const Fraction<Int> &f) {
    return f.num / f.denom;
}

template<class Int>
[[nodiscard]] inline Int ceil(const Fraction<Int> &f) {
    return f.num / f.denom + bool(f.num % f.denom);
}

template<class Int>
[[nodiscard]] inline Int round(const Fraction<Int> &f) {
    return f.num / f.denom + bool(f.num % f.denom * 2 >= f.denom);
}

};


#endif
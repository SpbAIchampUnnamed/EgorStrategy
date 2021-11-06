#ifndef __FRACTION_HPP__
#define __FRACTION_HPP__

#include <numeric>
#include <iostream>
#include <cmath>
#include <limits>
#include <concepts>
#include <compare>

template<std::signed_integral Int = int>
struct Fraction {
    Int num, denom;
    Fraction(Int num = 0, Int denom = 1): num(num), denom(denom) {
        normalize();
        normalize_sign();
    }
    template<std::signed_integral Int2>
    Fraction(Int2 num, Int2 denom = 1): num(num), denom(denom) {
        normalize();
        normalize_sign();
    }

    template<std::signed_integral Int2>
    explicit Fraction(const Fraction<Int2> &other) requires(sizeof(Int2) >= sizeof(Int)) : num(other.num), denom(other.denom) {}

    template<std::signed_integral Int2>
    Fraction(const Fraction<Int2> &other) requires(sizeof(Int2) < sizeof(Int)) : num(other.num), denom(other.denom) {}

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
            denom = 1;
            int exp = u.bits >> 52 & ((1ll << 11) - 1);
            exp -= 1023;
            exp -= 52;
            if constexpr (sizeof(Int) * 8 - 2 < 52) {
                mantisa >>= ((54 - sizeof(Int) * 8));
                exp += ((54 - sizeof(Int) * 8));
            }
            while ((mantisa & 1) == 0 && exp < 0) {
                mantisa >>= 1;
                ++exp;
            }
            num = mantisa;
            if (exp > 0) {
                num = std::numeric_limits<Int>::max();
                denom = 1;
            } else if (exp >= -(int) sizeof(Int) * 8 - 2) {
                denom <<= -exp;
            } else if (exp >= -2 * ((int) sizeof(Int) * 8 - 2)) {
                denom <<= sizeof(Int) * 8 - 2;
                num >>= -exp - (sizeof(Int) * 8 - 2);
            } else {
                num = 0;
            }
            while ((denom & 1) == 0 && (num & 1) == 0) {
                denom >>= 1;
                num >>= 1;
            }
            if (u.bits >> 63) {
                num = -num;
            }
        }
    }

    explicit operator double() const {
        return (double) num / (double) denom;
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

    void round_to_denom(Int new_denom) {
        if (denom <= new_denom) {
            return;
        }
        bool sign = (num < 0);
        if (sign)
            num = -num;
        Fraction l, r;
        l.num = 0;
        l.denom = 1;
        r.num = 1;
        r.denom = 0;
        Fraction m;
        while (l.denom + r.denom <= new_denom) {
            m.num = l.num + r.num;
            m.denom = l.denom + r.denom;
            if (m < *this) {
                l = m;
            } else {
                r = m;
            }
        }
        if (*this < m) {
            *this = l;
        } else if (*this > m) {
            *this = r;
        } else {
            *this = m;
        }
        if (sign)
            num = -num;
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
    friend std::strong_ordering operator<=>(Fraction a, Fraction b) {
        if ((a.num <=> 0) != (b.num <=> 0)) {
            return a.num <=> b.num;
        }
        if (a.num < 0) {
            a.num = -a.num;
            b.num = -b.num;
            std::swap(a, b);
        }
        while (1) {
            if (a.num == b.num && a.denom == b.denom) {
                return std::strong_ordering::equal;
            } else if (a.num / a.denom != b.num / b.denom) {
                return a.num / a.denom <=> b.num / b.denom;
            } else {
                a.num %= a.denom;
                b.num %= b.denom;
                if (a.num == 0 || b.num == 0) {
                    return a.num <=> b.num;
                }
                std::swap(a.num, b.denom);
                std::swap(a.denom, b.num);
            }
        }
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
#pragma once
#include "dspsim/signal.h"
#include <type_traits>
#include <iostream>

namespace dspsim
{
    template <typename T, size_t N = 1>
    class Input : public Model
    {
    protected:
        Signal<T> &sig;
        T &top_sig;

    public:
        Input(Signal<T> &_sig, T &_top_sig) : sig(_sig), top_sig(_top_sig)
        {
            top_sig = sig._read_d();
        }
        void eval_step() {}
        void eval_end_step()
        {
            top_sig = sig._read_d();
        }
    };
    template <typename T, size_t N>
    class Input<T[N]> : Model
    {
    protected:
        std::array<Signal<T> *, N> sig;
        T *top_sig[N];

    public:
        Input(std::array<Signal<T> *, N> &_sig, T (&_top_sig)[N]) : sig(_sig)
        {
            for (size_t i = 0; i < N; i++)
            {
                top_sig[i] = &_top_sig[i];
                *(top_sig[i]) = sig[i]->_read_d();
            }
        }
        void eval_step() {}
        void eval_end_step()
        {
            for (size_t i = 0; i < N; i++)
            {
                *(top_sig[i]) = sig[i]->_read_d();
            }
        }
    };

    template <typename T, size_t N, size_t M>
    class Input<T[N][M]> : public Model
    {
    protected:
        std::array<std::array<Signal<T> *, M>, N> sig;
        T *top_sig[N][M];

    public:
        Input(std::array<std::array<Signal<T> *, M>, N> &_sig, T (&_top_sig)[N][M]) : sig(_sig)
        {
            for (size_t i = 0; i < N; i++)
            {
                for (size_t j = 0; j < M; j++)
                {
                    top_sig[i][j] = &_top_sig[i][j];
                    *(top_sig[i][j]) = sig[i][j]->_read_d();
                }
            }
        }
        void eval_step() {}
        void eval_end_step()
        {
            for (size_t i = 0; i < N; i++)
            {
                for (size_t j = 0; j < M; j++)
                {
                    *(top_sig[i][j]) = sig[i][j]->_read_d();
                }
            }
        }
    };

    template <typename T>
    class Output
    {
    protected:
        Signal<T> &sig;
        T &top_sig;

    public:
        Output(Signal<T> &sig, T &top_sig) : sig(sig), top_sig(top_sig)
        {
            sig._bind(top_sig);
        }
    };

    template <typename T, size_t N>
    class Output<T[N]>
    {
    protected:
        std::array<Signal<T> *, N> sig;
        T *top_sig[N];

    public:
        Output(std::array<Signal<T> *, N> &sig, T (&_top_sig)[N]) : sig(sig)
        {
            for (size_t i = 0; i < N; i++)
            {
                // top_sig[i] = &_top_sig[i];
                // sig[i]->_force(*top_sig[i]);
                sig[i]->_bind(_top_sig[i]);
            }
        }
        // void eval_step() {}
        // void eval_end_step()
        // {
        //     for (size_t i = 0; i < N; i++)
        //     {
        //         sig[i]->_force(*top_sig[i]);
        //     }
        // }
    };

    template <typename T, size_t N, size_t M>
    class Output<T[N][M]>
    {
        using SignalArray = std::array<std::array<Signal<T> *, M>, N>;

    protected:
        SignalArray sig;
        T *top_sig[N][M];

    public:
        Output(SignalArray &sig, T (&_top_sig)[N][M]) : sig(sig)
        {
            for (size_t i = 0; i < N; i++)
            {
                for (size_t j = 0; j < M; j++)
                {
                    // top_sig[i][j] = &_top_sig[i][j];
                    // sig[i][j]->_force(*top_sig[i][j]);
                    sig[i][j]->_bind(_top_sig[i][j]); // ???
                }
            }
        }
        // void eval_step() {}
        // void eval_end_step()
        // {
        //     for (size_t i = 0; i < N; i++)
        //     {
        //         for (size_t j = 0; j < M; j++)
        //         {
        //             sig[i][j]->_force(*top_sig[i][j]);
        //         }
        //     }
        // }
    };

    // template <typename T>
    // using ptype = std::remove_reference_t<T>;
    // #define ptype(x) std::remove_reference_t<decltype(x)>
} // namespace dspsim

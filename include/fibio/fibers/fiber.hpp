//
//  fiber.hpp
//  fibio
//
//  Created by Chen Xu on 14-3-1.
//  Copyright (c) 2014 0d0a.com. All rights reserved.
//

#ifndef __fibio__fiber__
#define __fibio__fiber__

#include <memory>
#include <functional>
#include <chrono>
#include <utility>
#include <type_traits>
#include <boost/asio/io_service.hpp>
#include <fibio/fibers/detail/forward.hpp>
#include <fibio/fibers/detail/fiber_data.hpp>

namespace fibio { namespace fibers {
    /// struct scheduler
    class scheduler {
    public:
        /// constructor
        scheduler();
        
        /**
         * returns the io_service associated with the scheduler
         */
        boost::asio::io_service &get_io_service();
        
        /**
         * starts the scheduler with in a worker thread pool with specific size
         */
        void start(size_t nthr=1);

        /**
         * waits until the scheduler object stops
         */
        void join();
        
        /**
         * add work threads into the worker thread pool
         */
        void add_worker_thread(size_t nthr=1);
        
        /**
         * returns the scheduler singleton
         */
        static scheduler get_instance();

        /**
         * releases the scheduler singleton
         */
        static void reset_instance();
        
    private:
        scheduler(std::shared_ptr<detail::scheduler_object>);
        std::shared_ptr<detail::scheduler_object> impl_;
        friend class fiber;
    };
    
    /// struct fiber
    /**
     * manages a separate fiber
     */
    class fiber {
    public:
        /// fiber id type
        typedef uintptr_t id;
        
        /// fiber attributes
        struct attributes {
            // fiber scheduling policy
            /**
             * A fiber can be scheduled in two ways:
             * - normal: the fiber is freely scheduled across all
             *           worker threads
             * - stick_with_parent: the fiber always runs in the
             *                      same worker thread as its parent,
             *                      this can make sure the fiber and
             *                      its parent never run concurrently,
             *                      avoid some synchronizations for
             *                      shares resources
             */
            enum scheduling_policy {
                /**
                 * scheduled freely in this scheduler
                 */
                normal,

                /**
                 * always runs in the same thread with parent
                 */
                stick_with_parent,
            } policy;
            
            /// constructor
            constexpr attributes(scheduling_policy p) : policy(p) {}
        };
        
        /// Constructs new fiber object
        /**
         * Creates new fiber object which does not represent a fiber
         */
        fiber() = default;
        
        /// Move constructor
        /**
         * Constructs the fiber object to represent the fiber of
         * execution that was represented by other. After this
         * call other no longer represents a fiber of execution.
         */
        fiber(fiber &&other) noexcept;

        /**
         * Creates new fiber object and associates it with a fiber of execution.
         */
        template <class F>
        explicit fiber(F &&f)
        : data_(detail::make_fiber_data(detail::decay_copy(std::forward<F>(f))))
        { start(); }
        
        /**
         * Creates new fiber object and associates it with a fiber of execution.
         */
        template <class F, class Arg, class ...Args>
        fiber(F&& f, Arg&& arg, Args&&... args)
        : data_(detail::make_fiber_data(detail::decay_copy(std::forward<F>(f)),
                                        detail::decay_copy(std::forward<Arg>(arg)),
                                        detail::decay_copy(std::forward<Args>(args))...)
                )
        
        { start(); }
        
        /**
         * Creates new fiber object and associates it with a fiber of execution, using specific attributes
         */
        template <class F>
        fiber(attributes attrs, F &&f)
        : data_(detail::make_fiber_data(detail::decay_copy(std::forward<F>(f))))
        { start(attrs); }
        
        template <class F, class Arg, class ...Args>
        fiber(attributes attrs, F&& f, Arg&& arg, Args&&... args)
        : data_(detail::make_fiber_data(detail::decay_copy(std::forward<F>(f)),
                                        detail::decay_copy(std::forward<Arg>(arg)),
                                        detail::decay_copy(std::forward<Args>(args))...)
                )
        { start(attrs); }
        
        /**
         * Creates new fiber object and associates it with a fiber of execution in a specific scheduler.
         */
        template <class F>
        explicit fiber(scheduler &sched, F &&f)
        : data_(detail::make_fiber_data(detail::decay_copy(std::forward<F>(f))))
        { start(sched); }
        
        /**
         * Creates new fiber object and associates it with a fiber of execution in a specific scheduler.
         */
        template <class F, class Arg, class ...Args>
        fiber(scheduler &sched, F&& f, Arg&& arg, Args&&... args)
        : data_(detail::make_fiber_data(detail::decay_copy(std::forward<F>(f)),
                                        detail::decay_copy(std::forward<Arg>(arg)),
                                        detail::decay_copy(std::forward<Args>(args))...)
                )
        
        { start(sched); }
        
        /// Assigns the state of other to `*this` using move semantics.
        /**
         * If *this still has an associated running fiber (i.e. `joinable() == true`), `std::terminate()` is called.
         */
        fiber& operator=(fiber &&other) noexcept;

        /**
         * checks whether the fiber is joinable, i.e. potentially running in parallel context
         */
        bool joinable() const noexcept;
        
        /**
         * returns the _id_ of the thread
         */
        id get_id() const noexcept;
        
        /**
         * returns the number of concurrent fibers supported by the implementation
         */
        static unsigned hardware_concurrency() noexcept;
        
        /**
         * waits for a fiber to finish its execution
         */
        void join(bool propagate_exception=false);

        /**
         * permits the fiber to execute independently
         */
        void detach();
        
        /**
         * swaps two fiber objects
         */
        void swap(fiber &other) noexcept(true);
        
        /**
         * sets the name of the fiber
         */
        void set_name(const std::string &s);
        
        /**
         * returns the name of the fiber
         */
        std::string get_name();
        
    private:
        /// non-copyable
        fiber(const fiber&) = delete;
        void start();
        void start(attributes);
        void start(scheduler &sched);

        std::unique_ptr<detail::fiber_data_base> data_;
        std::shared_ptr<detail::fiber_object> impl_;
    };
    
    constexpr fiber::id not_a_fiber=0;
    
    namespace this_fiber {
        namespace detail {
            void sleep_usec(uint64_t usec);

            /**
             * returns the io_service associated with the current fiber
             */
            boost::asio::io_service &get_io_service();
        }
        

        /**
         * reschedule execution of fibers
         */
        void yield();
        
        /**
         * returns the fiber id of the current fiber
         */
        fiber::id get_id();
        
        /**
         * indicates if the current running context is a fiber
         */
        bool is_a_fiber() noexcept(true);
        
        /**
         * stops the execution of the current fiber for a specified time duration
         */
        template< class Rep, class Period >
        void sleep_for( const std::chrono::duration<Rep,Period>& sleep_duration ) {
            detail::sleep_usec(std::chrono::duration_cast<std::chrono::microseconds>(sleep_duration).count());
        }
        
        /**
         * stops the execution of the current fiber until a specified time point
         */
        template< class Clock, class Duration >
        void sleep_until( const std::chrono::time_point<Clock,Duration>& sleep_time ) {
            detail::sleep_usec(std::chrono::duration_cast<std::chrono::microseconds>(sleep_time - std::chrono::steady_clock::now()).count());
        }

        /**
         * get the name of current fiber
         */
        std::string get_name();

        /**
         * set the name of current fiber
         */
        void set_name(const std::string &name);
    }   // End of namespace this_fiber
}}   // End of namespace fibio::fibers

namespace std {
    /// specializes the std::swap algorithm for fiber objects
    inline void swap(fibio::fibers::fiber &lhs, fibio::fibers::fiber &rhs) noexcept(true) {
        lhs.swap(rhs);
    }
}   // End of namespace std

namespace fibio {
    typedef std::chrono::steady_clock::duration timeout_type;
    using fibers::scheduler;
    using fibers::fiber;
    namespace this_fiber {
        using fibers::this_fiber::yield;
        using fibers::this_fiber::get_id;
        using fibers::this_fiber::is_a_fiber;
        using fibers::this_fiber::sleep_for;
        using fibers::this_fiber::sleep_until;
        using fibers::this_fiber::get_name;
        using fibers::this_fiber::set_name;
    }
    namespace asio {
        using fibers::this_fiber::detail::get_io_service;
    }
}   // End of namespace fibio

#endif /* defined(__fibio__fiber__) */

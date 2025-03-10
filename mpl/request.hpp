#if !(defined MPL_REQUEST_HPP)

#define MPL_REQUEST_HPP

#include <mpi.h>
#include <utility>
#include <vector>

namespace mpl {

  class irequest;

  class prequest;

  class irequest_pool;

  class rrequest_pool;

  namespace impl {

    template<typename T>
    class base_request;

    template<typename T>
    class request_pool;

    class base_irequest {
      MPI_Request req = MPI_REQUEST_NULL;

    public:
      explicit base_irequest(MPI_Request req) : req(req) {}

      friend class base_request<base_irequest>;

      friend class request_pool<base_irequest>;
    };

    class base_prequest {
      MPI_Request req = MPI_REQUEST_NULL;

    public:
      explicit base_prequest(MPI_Request req) : req(req) {}

      friend class base_request<base_prequest>;

      friend class request_pool<base_prequest>;
    };

    //------------------------------------------------------------------

    template<typename T>
    class base_request {
    protected:
      MPI_Request req;

    public:
      base_request() = delete;

      base_request(const base_request &) = delete;

      explicit base_request(const base_irequest &req) : req{req.req} {}
      explicit base_request(const base_prequest &req) : req{req.req} {}

      base_request(base_request &&other) noexcept : req(other.req) {
        other.req = MPI_REQUEST_NULL;
      }

      ~base_request() {
        if (req != MPI_REQUEST_NULL)
          MPI_Request_free(&req);
      }

      void operator=(const base_request &) = delete;

      base_request &operator=(base_request &&other) noexcept {
        if (this != &other) {
          if (req != MPI_REQUEST_NULL)
            MPI_Request_free(&req);
          req = other.req;
          other.req = MPI_REQUEST_NULL;
        }
        return *this;
      }

      void cancel() { MPI_Cancel(&req); }

      std::pair<bool, status_t> test() {
        int result;
        status_t s;
        MPI_Test(&req, &result, static_cast<MPI_Status *>(&s));
        return std::make_pair(static_cast<bool>(result), s);
      }

      status_t wait() {
        status_t s;
        MPI_Wait(&req, static_cast<MPI_Status *>(&s));
        return s;
      }

      std::pair<bool, status_t> get_status() {
        int result;
        status_t s;
        MPI_Request_get_status(req, &result, static_cast<MPI_Status *>(&s));
        return std::make_pair(static_cast<bool>(result), s);
      }
    };

    //------------------------------------------------------------------

    template<typename T>
    class request_pool {
    protected:
      std::vector<MPI_Request> reqs;
      std::vector<status_t> stats;

    public:
      using size_type = std::vector<MPI_Request>::size_type;

      request_pool() = default;

      request_pool(const request_pool &) = delete;

      request_pool(request_pool &&other) noexcept
          : reqs(std::move(other.reqs)), stats(std::move(other.stats)) {}

      ~request_pool() {
        for (std::vector<MPI_Request>::iterator i(reqs.begin()), i_end(reqs.end()); i != i_end;
             ++i)
          if ((*i) != MPI_REQUEST_NULL)
            MPI_Request_free(&(*i));
      }

      void operator=(const request_pool &) = delete;

      request_pool &operator=(request_pool &&other) noexcept {
        if (this != &other) {
          for (std::vector<MPI_Request>::iterator i(reqs.begin()), i_end(reqs.end());
               i != i_end; ++i)
            if ((*i) != MPI_REQUEST_NULL)
              MPI_Request_free(&(*i));
          reqs = std::move(other.reqs);
          stats = std::move(other.stats);
        }
        return *this;
      }

      [[nodiscard]] size_type size() const { return reqs.size(); }

      [[nodiscard]] bool empty() const { return reqs.empty(); }

      [[nodiscard]] const status_t &get_status(size_type i) const { return stats[i]; }

      void cancel(size_type i) { MPI_Cancel(&reqs[i]); }

      void cancelall() {
        for (size_type i = 0; i < reqs.size(); ++i)
          cancel(i);
      }

      void push(T &&other) {
        reqs.push_back(other.req);
        other.req = MPI_REQUEST_NULL;
        stats.push_back(status_t());
      }

      std::pair<bool, size_type> waitany() {
        int index;
        status_t s;
        MPI_Waitany(size(), &reqs[0], &index, static_cast<MPI_Status *>(&s));
        if (index != MPI_UNDEFINED) {
          stats[index] = s;
          return std::make_pair(true, static_cast<size_type>(index));
        }
        return std::make_pair(false, size());
      }

      std::pair<bool, size_type> testany() {
        int index, flag;
        status_t s;
        MPI_Testany(size(), &reqs[0], &index, &flag, static_cast<MPI_Status *>(&s));
        if (flag != 0 and index != MPI_UNDEFINED) {
          stats[index] = s;
          return std::make_pair(true, static_cast<size_type>(index));
        }
        return std::make_pair(static_cast<bool>(flag), size());
      }

      void waitall() {
        MPI_Waitall(size(), &reqs[0], static_cast<MPI_Status *>(&stats[0]));
      }

      bool testall() {
        int flag;
        MPI_Testall(size(), &reqs[0], &flag, static_cast<MPI_Status *>(&stats[0]));
        return flag;
      }

      template<typename I>
      I waitsome(I iter) {
        mpl::detail::flat_memory_out<int, I> indices(size(), iter);
        int count;
        MPI_Waitsome(size(), &reqs[0], &count, indices.data(),
                     static_cast<MPI_Status *>(&stats[0]));
        if (count != MPI_UNDEFINED)
          return indices.copy_back(count);
        return iter;
      }

      template<typename I>
      I testsome(I iter) {
        mpl::detail::flat_memory_out<int, I> indices(size(), iter);
        int count;
        MPI_Testsome(size(), &reqs[0], &count, indices.data(),
                     static_cast<MPI_Status *>(&stats[0]));
        if (count != MPI_UNDEFINED)
          return indices.copy_back(count);
        return iter;
      }
    };

  }  // namespace impl

  //--------------------------------------------------------------------

  /// Represents a non-blocking communication request.
  class irequest : public impl::base_request<impl::base_irequest> {
    using base = impl::base_request<impl::base_irequest>;
    using base::req;

  public:
    irequest(const impl::base_irequest &r) : base(r) {}

    irequest(const irequest &) = delete;

    irequest(irequest &&r) noexcept : base(std::move(r)) {}

    void operator=(const irequest &) = delete;

    irequest &operator=(irequest &&other) noexcept {
      if (this != &other)
        base::operator=(std::move(other));
      return *this;
    }

    friend class impl::request_pool<irequest>;
  };

  //--------------------------------------------------------------------

  /// Container for managing a list of non-blocking communication requests.
  class irequest_pool : public impl::request_pool<irequest> {
    using base = impl::request_pool<irequest>;

  public:
    irequest_pool() = default;

    irequest_pool(const irequest_pool &) = delete;

    irequest_pool(irequest_pool &&r) noexcept : base(std::move(r)) {}

    void operator=(const irequest_pool &) = delete;

    irequest_pool &operator=(irequest_pool &&other) noexcept {
      if (this != &other)
        base::operator=(std::move(other));
      return *this;
    }
  };

  //--------------------------------------------------------------------

  /// Represents a persistent communication request.
  class prequest : public impl::base_request<impl::base_prequest> {
    using base = impl::base_request<impl::base_prequest>;
    using base::req;

  public:
    prequest(const impl::base_prequest &r) : base(r) {}

    prequest(const prequest &) = delete;

    prequest(prequest &&r) noexcept : base(std::move(r)) {}

    void operator=(const prequest &) = delete;

    prequest &operator=(prequest &&other) noexcept {
      if (this != &other)
        base::operator=(std::move(other));
      return *this;
    }

    void start() { MPI_Start(&req); }

    friend class impl::request_pool<prequest>;
  };

  //--------------------------------------------------------------------

  /// Container for managing a list of persisting communication requests.
  class prequest_pool : public impl::request_pool<prequest> {
    using base = impl::request_pool<prequest>;
    using base::reqs;

  public:
    prequest_pool() = default;

    void operator=(const prequest_pool &) = delete;

    prequest_pool &operator=(prequest_pool &&other) noexcept {
      if (this != &other)
        base::operator=(std::move(other));
      return *this;
    }

    prequest_pool(prequest_pool &&r) noexcept : base(std::move(r)) {}

    void startall() { MPI_Startall(size(), &reqs[0]); }
  };

}  // namespace mpl

#endif

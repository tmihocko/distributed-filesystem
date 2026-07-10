#ifndef SINGLETON_HPP
#define SINGLETON_HPP

template <typename T>
class Singleton {
  public:
	static T &shared() {
		static T instance;
		return instance;
	}

	Singleton(const Singleton &) = delete;
	Singleton &operator=(const Singleton &) = delete;
	Singleton(Singleton &&) = delete;
	Singleton &operator=(Singleton &&) = delete;

  protected:
	Singleton() {
	} // private constructor
};

#endif // SINGLETON_HPP

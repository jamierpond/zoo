#include "AnyExtensions.h"

TEST_CASE("IAnyContainer") {
    using namespace zoo;
    using Container = IAnyContainer<sizeof(void *), alignof(void *)>;

    alignas(Container) char bytes[sizeof(Container)];
    new(bytes) Container;
    auto &container = *reinterpret_cast<Container *>(bytes);
    auto setTo0x33 = [](auto ptr, auto count) {
        for(auto i = count; i--; ) {
            ptr[i] = 0x33;
        }
    };
    auto check0x33 = [](auto ptr, auto count) {
        for(auto i = count; i--; ) {
            if(0x33 != ptr[i]) { return false; }
        }
        return true;
    };
    setTo0x33(container.m_space, sizeof(container.m_space));
    SECTION("Performance issue: constructor changes bytes") {
        new(bytes) Container;
        CHECK(check0x33(container.m_space, sizeof(container.m_space)));
    }
    Container c;
    SECTION("Copy does not initialize destination") {
        c.copy(&container);
        // the binary representation of a valid IAnyContainer can't be all bytes
        // set to 0x33
        CHECK(!check0x33(bytes, sizeof(bytes)));
    }
    static_assert(noexcept(c.move(&container)), "move is noexcept(true)");
    SECTION("Move does not initialize destination") {
        c.move(&container);
        CHECK(!check0x33(bytes, sizeof(bytes)));
    }
    static_assert(noexcept(c.value()), "value may throw");
    const auto containerPtr = &container;
    static_assert(noexcept(containerPtr->nonEmpty()), "nonEmpty may throw");
    SECTION("nonEmpty is true") {
        CHECK(!container.nonEmpty());
    }
    static_assert(noexcept(containerPtr->type()), "type may throw");
    SECTION("Type info is not void") {
        CHECK(typeid(void) == containerPtr->type());
    }
}

TEST_CASE("Resolved bugs") {
    using namespace zoo;
    SECTION("ReferentialContainer not a BaseContainer") {
        ReferentialContainer<1, 1, char> c{'a'};
        using Base = BaseContainer<1, 1>;
        CHECK(dynamic_cast<Base *>(&c));
        c.destroy();
    }
}

void debug() {};

TEST_CASE("Any", "[contract][canonical]") {
    SECTION("Value Destruction") {
        int value;
        {
            zoo::Any a{Destructor{&value}};
            REQUIRE(zoo::isRuntimeValue<Destructor>(a));
            value = 0;
        }
        REQUIRE(1 == value);
    }
    SECTION("Referential Semantics - Alignment, Destruction") {
        int value;
        {
            zoo::Any a{D2{&value}};
            REQUIRE(zoo::isRuntimeReference<D2>(a));
            value = 0;
        }
        REQUIRE(1 == value);
    }
    SECTION("Referential Semantics - Size") {
        zoo::Any v{Big{}};
        REQUIRE(zoo::isRuntimeReference<Big>(v));
        REQUIRE(v.has_value());
    }
    SECTION("Copy constructor - value held is not an \"Any\"") {
        zoo::Any a{5};
        zoo::Any b{a};
        REQUIRE(zoo::isRuntimeValue<int>(b));
    }
    SECTION("Move constructor -- Value") {
        zoo::Any movingFrom{Moves{}};
        REQUIRE(zoo::isRuntimeValue<Moves>(movingFrom));
        zoo::Any movedTo{std::move(movingFrom)};
        auto ptrFrom = zoo::any_cast<Moves>(&movingFrom);
        auto ptrTo = zoo::any_cast<Moves>(&movedTo);
        REQUIRE(Moves::MOVED == ptrFrom->kind);
        REQUIRE(Moves::MOVING == ptrTo->kind);
    }
    SECTION("Move constructor -- Referential") {
        zoo::Any movingFrom{Big{}};
        REQUIRE(zoo::isRuntimeReference<Big>(movingFrom));
        auto original = zoo::any_cast<Big>(&movingFrom);
        zoo::Any movingTo{std::move(movingFrom)};
        auto afterMove = zoo::any_cast<Big>(&movingTo);
        bool shouldNotHaveValue = !movingFrom.has_value();
        CHECK(shouldNotHaveValue);
        REQUIRE(original == afterMove);
        REQUIRE(nullptr == zoo::any_cast<Big>(&movingFrom));
    }
    SECTION("Initializer constructor -- copying") {
        Moves value;
        zoo::Any copied{value};
        auto ptr = zoo::any_cast<Moves>(&copied);
        REQUIRE(Moves::COPIED == ptr->kind);
    }
    SECTION("Initializer constructor -- moving") {
        Moves def;
        CHECK(Moves::DEFAULT == def.kind);
        zoo::Any moving{std::move(def)};
        REQUIRE(Moves::MOVED == def.kind);
        REQUIRE(Moves::MOVING == zoo::any_cast<Moves>(&moving)->kind);
    }
    SECTION("Assignments") {
        using namespace zoo;
        zoo::Any integer{5};
        int willChange = 0;
        zoo::Any willBeTrampled{Destructor{&willChange}};
        willBeTrampled = integer;
        auto asInt = any_cast<int>(&willBeTrampled);
        REQUIRE(nullptr != asInt);
        REQUIRE(5 == *asInt);
        REQUIRE(1 == willChange);
        willChange = 0;
        zoo::Any anotherTrampled{D2{&willChange}};
        *asInt = 9;
        anotherTrampled = willBeTrampled;
        asInt = any_cast<int>(&anotherTrampled);
        REQUIRE(nullptr != asInt);
        REQUIRE(9 == *asInt);
        REQUIRE(1 == willChange);
        integer = Moves{};
        auto movPtr = any_cast<Moves>(&integer);
        REQUIRE(nullptr != movPtr);
        REQUIRE(Moves::MOVING == movPtr->kind);
        debug();
        willBeTrampled = *movPtr;
        auto movPtr2 = any_cast<Moves>(&willBeTrampled);
        REQUIRE(nullptr != movPtr2);
        REQUIRE(Moves::COPIED == movPtr2->kind);
        anotherTrampled = std::move(*movPtr2);
        REQUIRE(Moves::MOVED == movPtr2->kind);
        auto p = any_cast<Moves>(&anotherTrampled);
        REQUIRE(Moves::MOVING == p->kind);
    }
    zoo::Any empty;
    SECTION("reset()") {
        REQUIRE(!empty.has_value());
        empty = 5;
        REQUIRE(empty.has_value());
        empty.reset();
        REQUIRE(!empty.has_value());
    }
    SECTION("typeid") {
        REQUIRE(typeid(void) == empty.type());
        empty = Big{};
        REQUIRE(typeid(Big) == empty.type());
    }
    SECTION("swap") {
        zoo::Any other{5};
        anyContainerSwap(empty, other);
        REQUIRE(typeid(int) == empty.type());
        REQUIRE(typeid(void) == other.type());
        auto valuePointerAtEmpty = zoo::any_cast<int>(&empty);
        REQUIRE(5 == *valuePointerAtEmpty);
    }
    SECTION("any_cast") {
        REQUIRE_THROWS_AS(zoo::any_cast<int>(empty), std::bad_cast &);
        REQUIRE_THROWS_AS(zoo::any_cast<int>(empty), zoo::bad_any_cast);
        REQUIRE(nullptr == zoo::any_cast<int>(&empty));
        const zoo::Any *constAny = nullptr;
        REQUIRE(nullptr == zoo::any_cast<int>(constAny));
        constAny = &empty;
        empty = 7;
        REQUIRE(nullptr != zoo::any_cast<int>(constAny));
    }
    SECTION("inplace") {
        zoo::Any bfi{std::in_place_type<BuildsFromInt>, 5};
        REQUIRE(typeid(BuildsFromInt) == bfi.type());
        zoo::Any il{std::in_place_type<TakesInitializerList>, { 9, 8, 7 }, 2.2};
        REQUIRE(typeid(TakesInitializerList) == il.type());
        auto ptr = zoo::any_cast<TakesInitializerList>(&il);
        REQUIRE(3 == ptr->s);
        REQUIRE(2.2 == ptr->v);
    }
    SECTION("Multiple argument constructor -- value") {
        zoo::Any mac{TwoArgumentConstructor{nullptr, 3}};
        REQUIRE(zoo::isRuntimeValue<TwoArgumentConstructor>(mac));
        auto ptr = zoo::any_cast<TwoArgumentConstructor>(&mac);
        REQUIRE(false == ptr->boolean);
        REQUIRE(3 == ptr->value);
    }
}

TEST_CASE("AnyExtensions", "[contract]") {
    testAnyImplementation<
        zoo::AnyContainer<zoo::ConverterPolicy<8, 8>>
    >();
}

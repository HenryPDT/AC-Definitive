#pragma once
#include <cstdint>

namespace AC2
{
    /**
     * @brief ItemID enum containing all known inventory, gear, and model IDs
     * sourced 1-to-1 from Paul44's AC2 v9.2 Cheat Table.
     */
    enum class ItemID : uint32_t
    {
        // Consumables
        Bullets             = 0x144A31C0,
        Poison              = 0x144A31C5,
        ThrowingKnives      = 0xAFD4F6F3,
        SmokeBombs          = 0xAFD4F6F8,
        Medicine            = 0xC3C76292,

        // Outfits / Skins
        DefaultCape         = 0xCF2FB9FB,
        YoungEzio           = 0x64373864,
        AssassinWhite       = 0x1F573BB3,
        AltairArmor         = 0x10AA7D95,
        AltairRobes         = 0x1C95F1E1,
        AltairRobes2        = 0x8FFB2E81,
        BorgiaGuard         = 0x5B19B96E,
        Desmond             = 0x08683BB9,
        BonusSkin           = 0xB56553E8,

        // Armor - Greaves
        HelmschmiedGreaves  = 0x2E0957F7,
        MetalGreaves        = 0x2E0957F8,
        LeatherGreaves      = 0xB2186F0C,
        MissagliasGreaves   = 0xB2186F1D,

        // Armor - Chest Guards
        MissagliasChestGuard = 0x2E095802,
        MetalChestGuard      = 0x2E095807,
        HelmschmiedChestGuard = 0x2E09580C,
        LeatherChestGuard    = 0x2E095811,

        // Armor - Spaulders / Pauldrons
        HelmschmiedSpaulders = 0x2E095817,
        LeatherSpaulders     = 0x2E09581C,
        MetalPauldrons       = 0x2E095821,
        MissagliasPauldrons  = 0x2E095826,

        // Armor - Vambraces
        LeatherVambraces     = 0x2E09582D,
        HelmschmiedVambraces = 0x2E095832,
        MetalVambraces       = 0x2E095837,
        MissagliasVambraces  = 0x2E09583C,

        // Paintings
        PortraitOfAMusician  = 0x4787A82F,
        FrancescoDelleOpere  = 0x4787A834,
        StFrancisInEcstasy   = 0x4787A839,
        IdealCity            = 0x4787A83E,
        BattistaAndFederico  = 0x4787A843,
        SaintJeanBaptiste    = 0x4787A848,
        BaptismOfChrist      = 0x4787A84D,
        SaintChrysogonus     = 0x4787A866,
        LadyWithAnErmine     = 0x4787A87F,
        VenusRising          = 0x4787A884,
        SleepingVenus        = 0x4787A889,
        MadonnaAndChild      = 0x4787A88E,
        FedericoDaMontefeltro = 0x4787A893,
        SimonettaVespucci    = 0x4787A898,
        VenusAndTheMirror    = 0x4787A89D,
        PortraitOfALady      = 0x4787A8A2,
        Annunciation         = 0x4787A8A7,
        PallasAndTheCentaur  = 0x4787A8AC,
        Primavera            = 0x4787A8B1,

        // Dyes
        TuscanEmber          = 0x55308BA6,
        FlorentineCrimson    = 0x55308BAB,
        TuscanEmerald        = 0x55308BB0,
        FlorentineScarlet    = 0x55308BC4,
        FlorentineMahogany   = 0x55308BDD,
        TuscanCopper         = 0x55308BE2,
        TuscanOchre          = 0x55308BE7,

        // Pouches / Upgrades
        KnifeBeltUpgrade1    = 0x55308E80,
        KnifeBeltUpgrade2    = 0x55308E81,
        KnifeBeltUpgrade3    = 0x55308E82,
        MediumMedicinePouch  = 0x55308EA5,
        LargeMedicinePouch   = 0x55308EA6,
        LargePoisonVial      = 0x61D961DE,
        MediumPoisonVial     = 0x61D961DF,

        // Weapons - Small / Daggers
        WeaponChanneledCinquedea = 0x784EC401,
        WeaponStiletto           = 0x784EC406,
        WeaponButcherKnife       = 0x784EC40B,
        WeaponNotchedCinquedea   = 0x784EC410,
        WeaponSultansKnife       = 0x784EC415,
        WeaponVenetianFalchion   = 0x784EC41A,
        WeaponDagger             = 0x784EC41F,
        WeaponKnife              = 0x784EC424,

        // Weapons - Swords / Hammers / Maces
        WeaponCaptainsSword      = 0x784ECD7D,
        WeaponMaul               = 0x784ECD87,
        WeaponMercenarioWarHammer = 0x784ECD8C,
        WeaponSchiavona          = 0x784ECD91,
        WeaponMilaneseSword      = 0x784ECD96,
        WeaponFlangedMace        = 0x784ECD9B,
        WeaponFlorentineFalchion = 0x784ECDA0,
        WeaponScimitar           = 0x784ECDA5,
        WeaponCavalieriMace      = 0x784ECDAA,
        WeaponCondottieroWarHammer = 0x784ECDAF,
        WeaponSwordOfAltair      = 0x26740BA6,
        WeaponOldSyrianSword     = 0x26740BAB,
        WeaponMetalCestus        = 0x26740BD7,

        // Locations
        SanGiovanni          = 0x08A66423,
        SanMarco             = 0xAC75BA1A,
        SantaMariaNovella    = 0xAC75BA21,
        SanGimignano         = 0xAC75BA27,
        Tuscany              = 0xAC75BA2F,
        Monteriggioni        = 0xAC75BA62,
        ApennineMountains    = 0xAC75BA63,
    };
}
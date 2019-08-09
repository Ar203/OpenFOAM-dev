/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2013-2019 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "mixtureFraction.H"
#include "singleStepCombustion.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class ReactionThermo, class ThermoType>
Foam::radiationModels::sootModels::mixtureFraction<ReactionThermo, ThermoType>::
mixtureFraction
(
    const dictionary& dict,
    const fvMesh& mesh,
    const word& modelType
)
:
    sootModel(dict, mesh, modelType),
    soot_
    (
        IOobject
        (
            "soot",
            mesh_.time().timeName(),
            mesh_,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        mesh_
    ),
    coeffsDict_(dict.subOrEmptyDict(modelType + "Coeffs")),
    nuSoot_(readScalar(coeffsDict_.lookup("nuSoot"))),
    Wsoot_(readScalar(coeffsDict_.lookup("Wsoot"))),
    sootMax_(-1),
    mappingFieldName_
    (
        coeffsDict_.lookupOrDefault<word>("mappingField", "none")
    ),
    mapFieldMax_(1)
{
    const combustionModels::singleStepCombustion<ReactionThermo, ThermoType>&
    combustion
    (
        mesh.lookupObject
        <
            combustionModels::singleStepCombustion<ReactionThermo, ThermoType>
        >
        (
            combustionModel::combustionPropertiesName
        )
    );

    const multiComponentMixture<ThermoType>& mixture(combustion.mixture());
    const Reaction<ThermoType>& reaction = combustion.reaction();

    scalar totalMol = 0;
    forAll(reaction.rhs(), i)
    {
        const scalar stoichCoeff = reaction.rhs()[i].stoichCoeff;
        totalMol += mag(stoichCoeff);
    }

    totalMol += nuSoot_;

    scalarList Xi(reaction.rhs().size());

    scalar Wm = 0;
    forAll(reaction.rhs(), i)
    {
        const label speciei = reaction.rhs()[i].index;
        const scalar stoichCoeff = reaction.rhs()[i].stoichCoeff;
        Xi[i] = mag(stoichCoeff)/totalMol;
        Wm += Xi[i]*mixture.speciesData()[speciei].W();
    }

    scalarList Yprod0(mixture.species().size(), 0.0);

    forAll(reaction.rhs(), i)
    {
        const label speciei = reaction.rhs()[i].index;
        Yprod0[speciei] = mixture.speciesData()[speciei].W()/Wm*Xi[i];
    }

    const scalar XSoot = nuSoot_/totalMol;
    Wm += XSoot*Wsoot_;

    sootMax_ = XSoot*Wsoot_/Wm;

    Info << "Maximum soot mass concentrations: " << sootMax_ << nl;

    if (mappingFieldName_ == "none")
    {
        const label index = reaction.rhs()[0].index;
        mappingFieldName_ = mixture.Y(index).name();
    }

    const label mapFieldIndex = mixture.species()[mappingFieldName_];

    mapFieldMax_ = Yprod0[mapFieldIndex];
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

template<class ReactionThermo, class ThermoType>
Foam::radiationModels::sootModels::mixtureFraction<ReactionThermo, ThermoType>::
~mixtureFraction()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class ReactionThermo, class ThermoType>
void
Foam::radiationModels::sootModels::mixtureFraction<ReactionThermo, ThermoType>::
correct()
{
    const volScalarField& mapField =
        mesh_.lookupObject<volScalarField>(mappingFieldName_);

    soot_ = sootMax_*(mapField/mapFieldMax_);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "../NeuralAmpModeler/ToneStack.cpp"

namespace
{
struct StackCase
{
  ToneStackType type;
  const char* name;
  double bass;
  double mid;
  double treble;
};

double db(double value)
{
  return 20.0 * std::log10(std::max(1.0e-30, value));
}

int main_impl()
{
  constexpr double pi = 3.1415926535897932384626433832795;
  const std::vector<double> freqs{31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
  const std::vector<StackCase> cases{
    {ToneStackType::Hiwatt, "Hiwatt:noon", 5.0, 5.0, 5.0},
    {ToneStackType::Hiwatt, "Hiwatt:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::Hiwatt, "Hiwatt:bass5mid10", 5.0, 10.0, 5.0},
    {ToneStackType::Hiwatt, "Hiwatt:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::Hiwatt, "Hiwatt:mid10", 5.0, 10.0, 5.0},
    {ToneStackType::Hiwatt, "Hiwatt:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BlackstarHT5, "BlackstarHT5:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BlackstarHT5, "BlackstarHT5:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::BlackstarHT5, "BlackstarHT5:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::BlackstarHT5, "BlackstarHT5:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::BlackstarHT5, "BlackstarHT5:mid10", 5.0, 10.0, 5.0},
    {ToneStackType::BlackstarHT5, "BlackstarHT5:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BlackstarHT5, "BlackstarHT5:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BigMuffHoof, "BigMuffHoof:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BigMuffHoof, "BigMuffHoof:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::BigMuffHoof, "BigMuffHoof:mid10", 5.0, 10.0, 5.0},
    {ToneStackType::BigMuffHoof, "BigMuffHoof:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BigMuffHoof, "BigMuffHoof:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BigMuffMusket, "BigMuffMusket:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BigMuffMusket, "BigMuffMusket:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::BigMuffMusket, "BigMuffMusket:mid10", 5.0, 10.0, 5.0},
    {ToneStackType::BigMuffMusket, "BigMuffMusket:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BigMuffMusket, "BigMuffMusket:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BigMuffPickle, "BigMuffPickle:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BigMuffPickle, "BigMuffPickle:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::BigMuffPickle, "BigMuffPickle:mid10", 5.0, 10.0, 5.0},
    {ToneStackType::BigMuffPickle, "BigMuffPickle:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BigMuffPickle, "BigMuffPickle:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::Crate, "Crate:noon", 5.0, 5.0, 5.0},
    {ToneStackType::Crate, "Crate:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::Crate, "Crate:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::Crate, "Crate:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::DmblRock, "DmblRock:noon", 5.0, 5.0, 5.0},
    {ToneStackType::DmblRock, "DmblRock:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::DmblRock, "DmblRock:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::DmblRock, "DmblRock:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::DmblJazz, "DmblJazz:noon", 5.0, 5.0, 5.0},
    {ToneStackType::DmblJazz, "DmblJazz:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::DmblJazz, "DmblJazz:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::DmblJazz, "DmblJazz:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::FndrDeluxe5E3, "FndrDeluxe5E3:noon", 5.0, 5.0, 5.0},
    {ToneStackType::FndrDeluxe5E3, "FndrDeluxe5E3:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::FndrDeluxe5E3, "FndrDeluxe5E3:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::FndrDeluxe5E3, "FndrDeluxe5E3:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::FndrTMB, "FndrTMB:noon", 5.0, 5.0, 5.0},
    {ToneStackType::FndrTMB, "FndrTMB:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::FndrTMB, "FndrTMB:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::FndrTMB, "FndrTMB:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::FndrTMB, "FndrTMB:all0", 0.0, 0.0, 0.0},
    {ToneStackType::FndrTrebleBass, "FndrTrebleBass:noon", 5.0, 5.0, 5.0},
    {ToneStackType::FndrTrebleBass, "FndrTrebleBass:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::FndrTrebleBass, "FndrTrebleBass:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::SovtekMIG60, "SovtekMIG60:noon", 5.0, 5.0, 5.0},
    {ToneStackType::SovtekMIG60, "SovtekMIG60:all0", 0.0, 0.0, 0.0},
    {ToneStackType::SovtekMIG60, "SovtekMIG60:all10", 10.0, 10.0, 10.0},
    {ToneStackType::SovtekMIG100H, "SovtekMIG100H:noon", 5.0, 5.0, 5.0},
    {ToneStackType::SovtekMIG100H, "SovtekMIG100H:all0", 0.0, 0.0, 0.0},
    {ToneStackType::SovtekMIG100H, "SovtekMIG100H:all10", 10.0, 10.0, 10.0},
    {ToneStackType::DrZ, "DrZ:noon", 5.0, 5.0, 5.0},
    {ToneStackType::DrZ, "DrZ:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::DrZ, "DrZ:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::HiwattCP, "HiwattCP:noon", 5.0, 5.0, 5.0},
    {ToneStackType::HiwattCP, "HiwattCP:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::HiwattCP, "HiwattCP:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::HiwattCP, "HiwattCP:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::HiwattCP, "HiwattCP:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::Bandmaster6G7, "Bandmaster6G7:noon", 5.0, 5.0, 5.0},
    {ToneStackType::Bandmaster6G7, "Bandmaster6G7:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::Bandmaster6G7, "Bandmaster6G7:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::Bandmaster6G7, "Bandmaster6G7:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::Bandmaster6G7, "Bandmaster6G7:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BoneRay, "BoneRay:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BoneRay, "BoneRay:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::BoneRay, "BoneRay:mid10", 5.0, 10.0, 5.0},
    {ToneStackType::BoneRay, "BoneRay:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BoneRay, "BoneRay:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::Twin5D8, "Twin5D8:noon", 5.0, 5.0, 5.0},
    {ToneStackType::Twin5D8, "Twin5D8:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::Twin5D8, "Twin5D8:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::Twin5D8, "Twin5D8:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::Twin5D8, "Twin5D8:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::Aria, "Aria:noon", 5.0, 5.0, 5.0},
    {ToneStackType::Aria, "Aria:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::Aria, "Aria:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::Aria, "Aria:mid0", 5.0, 0.0, 5.0},
    {ToneStackType::Aria, "Aria:mid10", 5.0, 10.0, 5.0},
    {ToneStackType::Aria, "Aria:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::Aria, "Aria:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::JamesActiveDualBassCap, "JamesActiveDual:noon", 5.0, 5.0, 5.0},
    {ToneStackType::JamesActiveDualBassCap, "JamesActiveDual:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::JamesActiveDualBassCap, "JamesActiveDual:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::JamesActiveDualBassCap, "JamesActiveDual:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::JamesActiveDualBassCap, "JamesActiveDual:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::JamesActiveSingleBassCap, "JamesActiveSingle:noon", 5.0, 5.0, 5.0},
    {ToneStackType::JamesActiveSingleBassCap, "JamesActiveSingle:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::JamesActiveSingleBassCap, "JamesActiveSingle:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::JamesActiveSingleBassCap, "JamesActiveSingle:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::JamesActiveSingleBassCap, "JamesActiveSingle:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::JamesPassiveDualBassCap, "JamesPassiveDual:noon", 5.0, 5.0, 5.0},
    {ToneStackType::JamesPassiveDualBassCap, "JamesPassiveDual:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::JamesPassiveDualBassCap, "JamesPassiveDual:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::JamesPassiveDualBassCap, "JamesPassiveDual:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::JamesPassiveDualBassCap, "JamesPassiveDual:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::JamesPassiveSingleBassCap, "JamesPassiveSingle:noon", 5.0, 5.0, 5.0},
    {ToneStackType::JamesPassiveSingleBassCap, "JamesPassiveSingle:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::JamesPassiveSingleBassCap, "JamesPassiveSingle:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::JamesPassiveSingleBassCap, "JamesPassiveSingle:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::JamesPassiveSingleBassCap, "JamesPassiveSingle:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BaxandallPassiveDualBassCap, "BaxPassiveDual:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BaxandallPassiveDualBassCap, "BaxPassiveDual:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::BaxandallPassiveDualBassCap, "BaxPassiveDual:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::BaxandallPassiveDualBassCap, "BaxPassiveDual:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BaxandallPassiveDualBassCap, "BaxPassiveDual:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BaxandallPassiveSingleBassCap, "BaxPassiveSingle:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BaxandallPassiveSingleBassCap, "BaxPassiveSingle:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::BaxandallPassiveSingleBassCap, "BaxPassiveSingle:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::BaxandallPassiveSingleBassCap, "BaxPassiveSingle:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BaxandallPassiveSingleBassCap, "BaxPassiveSingle:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BaxandallActiveDualBassCap, "BaxActiveDual:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BaxandallActiveDualBassCap, "BaxActiveDual:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::BaxandallActiveDualBassCap, "BaxActiveDual:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::BaxandallActiveDualBassCap, "BaxActiveDual:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BaxandallActiveDualBassCap, "BaxActiveDual:treble10", 5.0, 5.0, 10.0},
    {ToneStackType::BaxandallActiveSingleBassCap, "BaxActiveSingle:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BaxandallActiveSingleBassCap, "BaxActiveSingle:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::BaxandallActiveSingleBassCap, "BaxActiveSingle:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::BaxandallActiveSingleBassCap, "BaxActiveSingle:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BaxandallActiveSingleBassCap, "BaxActiveSingle:treble10", 5.0, 5.0, 10.0},
  };

  std::cout << "case,freq,mag_db,phase_rad\n";
  std::cout << std::setprecision(12);
  for (const auto& item : cases)
  {
    const CircuitSpec spec = GetDefaultCircuitSpec(item.type);
    for (const double freq : freqs)
    {
      const Complex s(0.0, 2.0 * pi * freq);
      const Complex response = EvaluateToneStackMna(item.type, spec, item.bass, item.mid, item.treble, s);
      std::cout << item.name << "," << freq << "," << db(std::abs(response)) << "," << std::arg(response) << "\n";
    }
  }
  return 0;
}
} // namespace

int main()
{
  return main_impl();
}

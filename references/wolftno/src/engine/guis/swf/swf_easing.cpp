#include "engine/guis/swf/swf_easing.h"

#include <cmath>

namespace idSWFEasing {
float PI = 3.1415927f;

float Linear::ease(float t,float b,float c,float d){return t*c/d+b;}
float Back::easeIn(float t,float b,float c,float d){t/=d;return c*t*t*(2.70158f*t-1.70158f)+b;}
float Back::easeOut(float t,float b,float c,float d){t=t/d-1.0f;return c*(t*t*(2.70158f*t+1.70158f)+1.0f)+b;}
float Back::easeInOut(float t,float b,float c,float d){t/=d*0.5f;c*=0.5f;if(t<1.0f)return c*t*t*(3.5949094f*t-2.5949094f)+b;t-=2.0f;return c*(t*t*(3.5949094f*t+2.5949094f)+2.0f)+b;}
float Bounce::easeOut(float t,float b,float c,float d){t/=d;if(t<1.0f/2.75f)return c*7.5625f*t*t+b;if(t<2.0f/2.75f){t-=1.5f/2.75f;return c*(7.5625f*t*t+0.75f)+b;}if(t<2.5f/2.75f){t-=2.25f/2.75f;return c*(7.5625f*t*t+0.9375f)+b;}t-=2.625f/2.75f;return c*(7.5625f*t*t+0.984375f)+b;}
float Bounce::easeIn(float t,float b,float c,float d){return c-Bounce::easeOut(d-t,0.0f,c,d)+b;}
float Bounce::easeInOut(float t,float b,float c,float d){if(t<d*0.5f)return Bounce::easeIn(t*2.0f,0.0f,c,d)*0.5f+b;return Bounce::easeOut(t*2.0f-d,0.0f,c,d)*0.5f+c*0.5f+b;}
float Circ::easeIn(float t,float b,float c,float d){t/=d;return b-c*(std::sqrt(1.0f-t*t)-1.0f);}
float Circ::easeOut(float t,float b,float c,float d){t=t/d-1.0f;return c*std::sqrt(1.0f-t*t)+b;}
float Circ::easeInOut(float t,float b,float c,float d){t/=d*0.5f;c*=0.5f;if(t<1.0f)return b-c*(std::sqrt(1.0f-t*t)-1.0f);t-=2.0f;return c*(std::sqrt(1.0f-t*t)+1.0f)+b;}
float Cubic::easeIn(float t,float b,float c,float d){t/=d;return c*t*t*t+b;}
float Cubic::easeOut(float t,float b,float c,float d){t=t/d-1.0f;return c*(t*t*t+1.0f)+b;}
float Cubic::easeInOut(float t,float b,float c,float d){t/=d*0.5f;c*=0.5f;if(t<1.0f)return c*t*t*t+b;t-=2.0f;return c*(t*t*t+2.0f)+b;}
float Elastic::easeIn(float t,float b,float c,float d){if(t==0.0f)return b;t/=d;if(t==1.0f)return b+c;t-=1.0f;const float p=d*0.30000001f;return b-c*std::pow(2.0f,10.0f*t)*std::sin((t*d-p*0.25f)*(PI*2.0f)/p);}
float Elastic::easeOut(float t,float b,float c,float d){if(t==0.0f)return b;t/=d;if(t==1.0f)return b+c;const float p=d*0.30000001f;return c*std::pow(2.0f,-10.0f*t)*std::sin((t*d-p*0.25f)*(PI*2.0f)/p)+c+b;}
float Elastic::easeInOut(float t,float b,float c,float d){if(t==0.0f)return b;t/=d*0.5f;if(t==2.0f)return b+c;t-=1.0f;const float p=d*0.45000002f;const float wave=std::sin((t*d-p*0.25f)*(PI*2.0f)/p);if(t<0.0f)return b-0.5f*c*std::pow(2.0f,10.0f*t)*wave;return b+c+0.5f*c*std::pow(2.0f,-10.0f*t)*wave;}
float Expo::easeIn(float t,float b,float c,float d){return t==0.0f?b:c*std::pow(2.0f,10.0f*(t/d-1.0f))+b;}
float Expo::easeOut(float t,float b,float c,float d){return t==d?b+c:c*(1.0f-std::pow(2.0f,-10.0f*t/d))+b;}
float Expo::easeInOut(float t,float b,float c,float d){if(t==0.0f)return b;if(t==d)return b+c;t/=d*0.5f;c*=0.5f;if(t<1.0f)return c*std::pow(2.0f,10.0f*(t-1.0f))+b;return c*(2.0f-std::pow(2.0f,-10.0f*(t-1.0f)))+b;}
float Quad::easeIn(float t,float b,float c,float d){t/=d;return c*t*t+b;}
float Quad::easeOut(float t,float b,float c,float d){t/=d;return b-c*t*(t-2.0f);}
float Quad::easeInOut(float t,float b,float c,float d){t/=d*0.5f;c*=0.5f;if(t<1.0f)return c*t*t+b;t-=1.0f;return b-c*(t*(t-2.0f)-1.0f);}
float Quart::easeIn(float t,float b,float c,float d){t/=d;return c*t*t*t*t+b;}
float Quart::easeOut(float t,float b,float c,float d){t=t/d-1.0f;return b-c*(t*t*t*t-1.0f);}
float Quart::easeInOut(float t,float b,float c,float d){t/=d*0.5f;c*=0.5f;if(t<1.0f)return c*t*t*t*t+b;t-=2.0f;return b-c*(t*t*t*t-2.0f);}
float Quint::easeIn(float t,float b,float c,float d){t/=d;return c*t*t*t*t*t+b;}
float Quint::easeOut(float t,float b,float c,float d){t=t/d-1.0f;return c*(t*t*t*t*t+1.0f)+b;}
float Quint::easeInOut(float t,float b,float c,float d){t/=d*0.5f;c*=0.5f;if(t<1.0f)return c*t*t*t*t*t+b;t-=2.0f;return c*(t*t*t*t*t+2.0f)+b;}
float Sine::easeIn(float t,float b,float c,float d){return c-c*std::cos(PI*0.5f*t/d)+b;}
float Sine::easeOut(float t,float b,float c,float d){return c*std::sin(PI*0.5f*t/d)+b;}
float Sine::easeInOut(float t,float b,float c,float d){return b-(std::cos(PI*t/d)-1.0f)*(c*0.5f);}
} // namespace idSWFEasing
